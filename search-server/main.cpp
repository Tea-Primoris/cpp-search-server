//#include "search_server.h"
//
//#include <iostream>
//#include <string>
//#include <vector>
//
//using namespace std;
//
//int main() {
//    SearchServer search_server("and with"s);
//
//    int id = 0;
//    for (
//        const string& text : {
//            "funny pet and nasty rat"s,
//            "funny pet with curly hair"s,
//            "funny pet and not very nasty rat"s,
//            "pet with rat and rat and rat"s,
//            "nasty rat with curly hair"s,
//    }
//            ) {
//        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
//    }
//
//    const string query = "curly and funny"s;
//
//    auto report = [&search_server, &query] {
//        cout << search_server.GetDocumentCount() << " documents total, "s
//             << search_server.FindTopDocuments(query).size() << " documents for query ["s << query << "]"s << endl;
//    };
//
//    report();
//    // однопоточная версия
//    search_server.RemoveDocument(5);
//    report();
//    // однопоточная версия
//    search_server.RemoveDocument(execution::seq, 1);
//    report();
//    // многопоточная версия
//    search_server.RemoveDocument(execution::par, 2);
//    report();
//
//    return 0;
//}

#include "search_server.h"

#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "log_duration.h"

using namespace std;

string GenerateWord(mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int max_word_count) {
    const int word_count = uniform_int_distribution(1, max_word_count)(generator);
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

template <typename ExecutionPolicy>
void Test(string_view mark, SearchServer search_server, ExecutionPolicy&& policy) {
    LOG_DURATION_STREAM(string(mark), std::cout);
    const int document_count = search_server.GetDocumentCount();
    for (int id = 0; id < document_count; ++id) {
        search_server.RemoveDocument(policy, id);
    }
    cout << search_server.GetDocumentCount() << endl;
}

#define TEST(mode) Test(#mode, search_server, execution::mode)

int main() {
    mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 10'000, 25);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 100);

    {
        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        TEST(seq);
    }
    {
        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        TEST(par);
    }
}