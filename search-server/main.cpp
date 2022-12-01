#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string str;
    getline(cin, str);
    return str;
}

int ReadInt() {
    int result = 0;
    cin >> result;
    return result;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }

    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

struct Document {
    int id;
    double relevance;
    int rating;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, const DocumentStatus& status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double tf_one_word = 1.0 / words.size();
        for (const string& word : words) {
            index_[word][document_id] += tf_one_word;
        }
        document_ratings_.emplace(document_id, ComputeAverageRating(ratings));
        document_statuses_.emplace(document_id, status);
    }

    template<typename TFilter>
    vector<Document> FindTopDocuments(const string& raw_query, TFilter filter) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, filter);
        const double EPSILON = 1e-6;

        sort(matched_documents.begin(), matched_documents.end(),
            [&EPSILON](const Document& lhs, const Document& rhs) {
                return ((abs(lhs.relevance - rhs.relevance) < EPSILON) && lhs.rating > rhs.rating)  || (lhs.relevance > rhs.relevance);
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }

    int GetDocumentCount() const {
        return document_ratings_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        set<string> matched_plus_words;
        Query query = ParseQuery(raw_query);
        bool contains_minus_word = false;

        for (const string& minus_word : query.minus_words) {
            if (index_.find(minus_word) != index_.end() && index_.at(minus_word).count(document_id)) {
                contains_minus_word = true;
            }
        }

        if (!contains_minus_word)
        {
            for (const string& plus_word : query.words) {
                if (index_.find(plus_word) != index_.end() && index_.at(plus_word).count(document_id)) {
                    matched_plus_words.emplace(plus_word);
                }
            }
        }
        vector<string> plus_words_vector(matched_plus_words.begin(), matched_plus_words.end());
        return {plus_words_vector, document_statuses_.at(document_id)};
    }

private:
    set<string> stop_words_;
    map<string, map<int, double>> index_;
    map<int, int> document_ratings_;
    map<int, DocumentStatus> document_statuses_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }

        if (!ratings.empty()) {
            return rating_sum / static_cast<int>(ratings.size());
        }

        return 0;
    }

    struct Query {
        set<string> words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-') {
                query.minus_words.insert(word.substr(1));
            }
            else {
                query.words.insert(word);
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(static_cast<double>(document_ratings_.size()) / index_.at(word).size());
    }

    template<typename TFilter>
    vector<Document> FindAllDocuments(const Query& query, TFilter filter) const {
        map<int, double> matched_index;
        for (const string& plus_word : query.words) {
            if (index_.find(plus_word) != index_.end()) {
                const double idf = ComputeWordInverseDocumentFreq(plus_word);
                for (const auto& [id, tf] : index_.at(plus_word)) {
                    matched_index[id] += tf * idf;
                }
            }
        }

        for (const string& minus_word : query.minus_words) {
            if (index_.find(minus_word) != index_.end()) {
                for (const auto& [id, tf] : index_.at(minus_word)) {
                    matched_index.erase(id);
                }
            }
        }

        vector<Document> matched_documents;
        for (const auto& [id, relevance] : matched_index) {
            const int rating = document_ratings_.at(id);
            if (filter(id, document_statuses_.at(id), rating)) {
                matched_documents.push_back({ id, relevance, rating });
            }
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}
int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "ACTUAL:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; })) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}