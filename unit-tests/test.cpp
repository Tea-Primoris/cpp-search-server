#include "pch.h"
#include "../search-server/search_server.h"
#include "../search-server/search_server.cpp"
#include "../search-server/document.h"
#include "../search-server/document.cpp"
#include "../search-server/string_processing.h"
#include "../search-server/string_processing.cpp"

using namespace std::literals::string_literals;
TEST(SearchServer, ExcludeStopWordsFromAddedDocumentContent) {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQ(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQ(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_TRUE(server.FindTopDocuments("in"s).empty());
    }
}

TEST(SearchServer, ExcludeDocumentsWithMinusWords) {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "пушистый ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    for (const Document& document : search_server.FindTopDocuments("пушистый кот -хвост"s)) {
        ASSERT_TRUE(document.id != 1);
    }
}

TEST(SearchServer, DocumentMatching) {
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        auto [matched_words, status] = search_server.MatchDocument("модный белый кот"s, 0);
        ASSERT_EQ(matched_words.size(), 3);
    }

    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        auto [matched_words, status] = search_server.MatchDocument("модный белый -кот"s, 0);
        ASSERT_TRUE(matched_words.empty());
    }
}

TEST(SearchServer, SortingByRelevancy) {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_TRUE(result.at(0).relevance > result.at(1).relevance && result.at(1).relevance > result.at(2).relevance);
}

TEST(SearchServer, RelevancyCalc) {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    constexpr double EPSILON = 1e-6;
    ASSERT_TRUE(result.at(0).relevance - 0.866434 < EPSILON);
}

TEST(SearchServer, RatingCalc) {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_EQ(result.at(0).rating, 5);
}

TEST(SearchServer, SearchByStatus) {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
    ASSERT_EQ(result.at(0).id, 3);
}

TEST(SearchServer, UserPredicate) {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_TRUE(result.at(0).id == 0 && result.at(1).id == 2);
}

TEST(SearchServer, ConstructorWithStopWordsString) {
    {
        SearchServer search_server("и в на"s);
        std::set<std::string> expected_stop_words{ "и"s, "в"s, "на"s };
        ASSERT_EQ(expected_stop_words, search_server.GetStopWords());
    }

    {
        SearchServer search_server("  и в   на  "s);
        std::set<std::string> expected_stop_words{ "и"s, "в"s, "на"s };
        ASSERT_EQ(expected_stop_words, search_server.GetStopWords());
    }

    {
        SearchServer search_server("и   в   на     "s);
        std::set<std::string> expected_stop_words{ "и"s, "в"s, "на"s };
        ASSERT_EQ(expected_stop_words, search_server.GetStopWords());
    }
}

TEST(SearchServer, ConstructorWithStopWordsContainer) {
    {
        std::set<std::string> expected_stop_words{ "и"s, "в"s, "на"s };
        SearchServer search_server(expected_stop_words);
        ASSERT_EQ(expected_stop_words, search_server.GetStopWords());
    }

    {
        std::vector<std::string> stop_words{ "и"s, "в"s, "на"s, "на"s, "и"s };
        std::set<std::string> expected_stop_words{ "и"s, "в"s, "на"s };
        SearchServer search_server("и в на"s);
        ASSERT_EQ(expected_stop_words, search_server.GetStopWords());
    }
}