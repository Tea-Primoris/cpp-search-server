#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadInt() {
    int i;
    cin >> i;
    return i;
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

    void AddDocument(int document_id, const string& document, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double tf_one_word = 1.0 / words.size();
        for (const string& word : words) {
            index_[word][document_id] += tf_one_word;
        }

        document_ratings_[document_id] = ComputeAverageRating(ratings);

        ++document_count_;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                return lhs.relevance > rhs.relevance;
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    struct Query {
        set<string> words;
        set<string> minus_words;
    };

    map<string, map<int, double>> index_;
    map<int, int> document_ratings_;
    int document_count_ = 0;

    set<string> stop_words_;

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

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> matched_index;
        for (const string& plus_word : query.words) {
            if (index_.find(plus_word) != index_.end()) {
                const double idf = log((double)document_count_ / index_.at(plus_word).size());
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
            int rating = document_ratings_.at(id);
            matched_documents.push_back({ id, relevance, rating });
        }
        return matched_documents;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        int sum = 0;
        for (const int& rating : ratings) {
            sum += rating;
        }

        if (!ratings.empty()) {
            return sum / (int)ratings.size();
        }
        else {
            return 0;
        }
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        const string document_text = ReadLine();
        
        const int ratings_count = ReadInt();
        vector<int> ratings(ratings_count, 0);
        for (int& rating : ratings) {
            rating = ReadInt();
        }

        ReadLine(); //Если не прочитать строку, то последующий ввод прочитан не будет.

        search_server.AddDocument(document_id, document_text, ratings);
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance, rating] : search_server.FindTopDocuments(query)) {
        cout << "{ "s
             << "document_id = "s << document_id << ", "s
             << "relevance = "s << relevance << ", "s
             << "rating = "s << rating
             << " }"s << endl;
    }
}