#include "catch.hpp"

#include "../search-server/search_server.h"
#include "../search-server/search_server.cpp"
#include "../search-server/document.h"
#include "../search-server/document.cpp"
#include "../search-server/string_processing.h"
#include "../search-server/string_processing.cpp"

using namespace std::literals::string_literals;

TEST_CASE("String processing", "[string processing]") {
    SECTION("Split words in string") {
        std::string input = "Hello everybody my name is Markiplier"s;
        std::vector<std::string> expected = {"Hello"s, "everybody"s, "my"s, "name"s, "is"s, "Markiplier"s};
        REQUIRE(expected == SplitIntoWords(input));
    }

    SECTION("Split words in string with extra spaces") {
        std::string input = "   Hello      everybody     my     name         is            Markiplier        "s;
        std::vector<std::string> expected = {"Hello"s, "everybody"s, "my"s, "name"s, "is"s, "Markiplier"s};
        REQUIRE(expected == SplitIntoWords(input));
    }

    SECTION("Split words in string_view") {
        std::string input_string = "Hello everybody my name is Markiplier"s;
        std::string_view input(input_string);
        std::vector<std::string_view> expected = {input.substr(0, 5), input.substr(6, 9), input.substr(16, 2), input.substr(19, 4), input.substr(24, 2), input.substr(27, 10)};
        REQUIRE(expected == SplitIntoWords(input));
    }

    SECTION("Split words in string_view with extra spaces") {
        std::string input_string = "   Hello    everybody  my      name      is    Markiplier      "s;
        std::string_view input(input_string);
        std::vector<std::string_view> expected = {input.substr(3, 5), input.substr(12, 9), input.substr(23, 2), input.substr(31, 4), input.substr(41, 2), input.substr(47, 10)};
        REQUIRE(expected == SplitIntoWords(input));
    }
}

TEST_CASE("Search server", "[search server]") {
    SECTION("Exclude stop words from added document content") {
        const int doc_id = 42;
        const std::string content = "cat in the city"s;
        const std::vector<int> ratings = { 1, 2, 3 };

        {
            SearchServer server;
            server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
            const auto found_docs = server.FindTopDocuments("in"s);
            REQUIRE(found_docs.size() == 1);
            const Document& doc0 = found_docs[0];
            REQUIRE(doc0.id == doc_id);
        }

        {
            SearchServer server("in the"s);
            server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
            REQUIRE(server.FindTopDocuments("in"s).empty());
        }
    }

    SECTION("Exclude documents with minus words") {
        SearchServer search_server;
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument(2, "пушистый ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        for (const Document& document : search_server.FindTopDocuments("пушистый кот -хвост"s)) {
            REQUIRE(document.id != 1);
        }
    }

    SECTION("Document matching") {
        {
            SearchServer search_server("и в на"s);
            search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
            auto [matched_words, status] = search_server.MatchDocument("модный белый кот"s, 0);
            REQUIRE(matched_words.size() == 3);
        }

        {
            SearchServer search_server("и в на"s);
            search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
            auto [matched_words, status] = search_server.MatchDocument("модный белый -кот"s, 0);
            REQUIRE(matched_words.empty());
        }
    }

    SECTION("Sorting by relevancy") {
        SearchServer search_server("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        REQUIRE((result.at(0).relevance > result.at(1).relevance && result.at(1).relevance > result.at(2).relevance));
    }

    SECTION("Relevancy calculation") {
        SearchServer search_server("и в на"s);
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        constexpr double EPSILON = 1e-6;
        REQUIRE(result.at(0).relevance - 0.866434 < EPSILON);
    }

    SECTION("Rating calculation") {
        SearchServer search_server("и в на"s);
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        REQUIRE(result.at(0).rating == 5);
    }

    SECTION("Search by status") {
        SearchServer search_server("и в на"s);
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
        auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
        REQUIRE(result.at(0).id == 3);
    }

    SECTION("Search by predicate") {
        SearchServer search_server("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
        auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        REQUIRE(result.at(0).id == 0);
        REQUIRE(result.at(1).id == 2);
    }

    SECTION("Constructor with stop words string") {
        {
            SearchServer search_server("и в на"s);
            std::set<std::string> expected_stop_words{ "и"s, "в"s, "на"s };
            REQUIRE(expected_stop_words == search_server.GetStopWords());
        }

        {
            SearchServer search_server("  и в   на  "s);
            std::set<std::string> expected_stop_words{ "и"s, "в"s, "на"s };
            REQUIRE(expected_stop_words == search_server.GetStopWords());
        }

        {
            SearchServer search_server("и   в   на     "s);
            std::set<std::string> expected_stop_words{ "и"s, "в"s, "на"s };
            REQUIRE(expected_stop_words == search_server.GetStopWords());
        }
    }

    SECTION("Constructor with stop words container") {
        {
            std::set<std::string> expected_stop_words{ "и"s, "в"s, "на"s };
            SearchServer search_server(expected_stop_words);
            REQUIRE(expected_stop_words == search_server.GetStopWords());
        }

        {
            std::vector<std::string> stop_words{ "и"s, "в"s, "на"s, "на"s, "и"s };
            std::set<std::string> expected_stop_words{ "и"s, "в"s, "на"s };
            SearchServer search_server("и в на"s);
            REQUIRE(expected_stop_words == search_server.GetStopWords());
        }
    }
}