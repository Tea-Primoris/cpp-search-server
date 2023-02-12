#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <set>
#include <map>
#include <numeric>
#include <algorithm>
#include <cmath>

#include "document.h"
#include "string_processing.h"


class SearchServer {
public:

    void SetStopWords(const std::string& text);
    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    template<typename TFilter>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, TFilter filter) const;
    std::vector<Document> FindTopDocuments(const std::string& raw_query, const DocumentStatus& status = DocumentStatus::ACTUAL) const;
    int GetDocumentCount() const;
    std::set<std::string> GetStopWords() const;
    int GetDocumentId(int index) const;
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    SearchServer();
    explicit SearchServer(const std::string& stop_words);

    template<typename T>
    SearchServer(const T& stop_words_container) {
        for (const std::string stop_word : stop_words_container) {
            if (!stop_word.empty())
            {
                if (!IsValidWord(stop_word)) {
                    throw std::invalid_argument("Contains special symbols");
                }
                stop_words_.insert(stop_word);
            }
        }
    }

private:
    const int MAX_RESULT_DOCUMENT_COUNT = 5;
    struct DocumentInfo;
    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> index_;
    std::map<int, DocumentInfo> documents_info_;
    std::vector<int> document_ids_;

    struct WordInfo
    {
        std::string word = "";
        bool is_plus_word = false;
        bool is_minus_word = false;
        bool is_stop_word = false;
    };

    bool IsStopWord(const std::string& word) const;
    bool IsMinusWord(const std::string& word) const;
    std::vector<WordInfo> ParseWords(const std::string& text) const;
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct Query {
        std::set<std::string> words;
        std::set<std::string> minus_words;
    };

    struct DocumentInfo {
        int rating;
        DocumentStatus status;
    };

    Query ParseQuery(const std::string& text) const;
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template<typename TFilter>
    std::vector<Document> FindAllDocuments(const Query& query, TFilter filter) const;

    static bool IsValidWord(const std::string& word);
};

#include "search_server.tpp"