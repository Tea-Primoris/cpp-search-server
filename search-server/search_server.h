#pragma once
#include <algorithm>
#include <cmath>
#include <execution>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"


class SearchServer {
public:
    struct DocumentInfo {
        int rating;
        DocumentStatus status;
        std::map<std::string, double> freqs_of_words;
        std::vector<std::string> content;
    };

    void SetStopWords(const std::string& text);

//    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

    template<typename TFilter>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, TFilter filter) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, const DocumentStatus& status = DocumentStatus::ACTUAL) const;
    template<typename TFilter>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view& raw_query, TFilter filter) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view& raw_query, const DocumentStatus& status = DocumentStatus::ACTUAL) const;

    template<typename TFilter>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view& raw_query, TFilter filter) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view& raw_query, const DocumentStatus& status = DocumentStatus::ACTUAL) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string_view& raw_query, int document_id) const;

    int GetDocumentCount() const;
    std::set<std::string> GetStopWords() const;
    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::sequenced_policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy, int document_id);

    auto begin() const->std::set<int>::const_iterator;
    auto end() const->std::set<int>::const_iterator;

    auto documents_info_begin() -> std::map<int, DocumentInfo>::iterator;
    auto documents_info_end() -> std::map<int, DocumentInfo>::iterator;

    SearchServer();
    explicit SearchServer(const std::string& stop_words);
    explicit SearchServer(const std::string_view stop_words);
    template<typename T>
    SearchServer(const T& stop_words_container);

private:
    const int MAX_RESULT_DOCUMENT_COUNT = 5;
    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> index_;
    std::set<int> document_ids_;
    std::map<int, DocumentInfo> documents_info_;

    struct WordInfo
    {
        std::string_view word = "";
        bool is_plus_word = false;
        bool is_minus_word = false;
        bool is_stop_word = false;
    };

    bool IsStopWord(const std::string_view& word) const;
    bool IsMinusWord(const std::string_view& word) const;

    std::vector<WordInfo> ParseWords(const std::string_view& text) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view& text, bool sort_results = true) const;

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template<typename TFilter>
    std::vector<Document> FindAllDocuments(const Query& query, TFilter filter) const;

    template<typename TFilter>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query& query, TFilter filter) const;

    static bool IsValidWord(const std::string_view& word);
};


//Def

template<typename TFilter>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, TFilter filter) const {
    return FindTopDocuments(std::execution::seq, raw_query, filter);
}

template<typename TFilter>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, const std::string_view& raw_query, TFilter filter) const {
    //LOG_DURATION_STREAM("Operation time", std::cout);
    Query query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(query, filter);
    const double EPSILON = 1e-6;

    sort(matched_documents.begin(), matched_documents.end(),
         [&EPSILON](const Document& lhs, const Document& rhs) {
             return ((std::abs(lhs.relevance - rhs.relevance) < EPSILON) && lhs.rating > rhs.rating) || (lhs.relevance > rhs.relevance);
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template<typename TFilter>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, const std::string_view& raw_query, TFilter filter) const {
    Query query = ParseQuery(raw_query);

    std::vector<Document> matched_documents = FindAllDocuments(std::execution::par, query, filter);
    const double EPSILON = 1e-6;

    std::sort(std::execution::par, matched_documents.begin(), matched_documents.end(),
         [&EPSILON](const Document& lhs, const Document& rhs) {
             return ((std::abs(lhs.relevance - rhs.relevance) < EPSILON) && lhs.rating > rhs.rating) || (lhs.relevance > rhs.relevance);
         });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template<typename TFilter>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, TFilter filter) const {
    std::map<int, double> matched_index;

    std::for_each(std::execution::seq, query.plus_words.begin(), query.plus_words.end(), [&](const std::string_view& plus_word){
        const auto& matched_word = index_.find(std::string(plus_word));
        if (matched_word != index_.end()) {
            const double idf = ComputeWordInverseDocumentFreq(std::string(plus_word));
            for (const auto& [id, tf] : index_.at(std::string(plus_word))) {
                const DocumentInfo& document_info = documents_info_.at(id);
                if (filter(id, document_info.status, document_info.rating)) {
                    matched_index[id] += tf * idf;
                }
            }
        }
    });

    std::for_each(std::execution::seq, query.minus_words.begin(), query.minus_words.end(), [&](const std::string_view& minus_word){
        const auto& matched_word = index_.find(std::string(minus_word));
        if (matched_word != index_.end()) {
            for (const auto& [id, tf] : matched_word->second) {
                matched_index.erase(id);
            }
        }
    });

    std::vector<Document> matched_documents;
    matched_documents.reserve(matched_index.size());
    for (const auto& [id, relevance] : matched_index) {
        const DocumentInfo& document_info = documents_info_.at(id);
        matched_documents.push_back({ id, relevance, document_info.rating });
    }
    return matched_documents;
}

template<typename TFilter>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query, TFilter filter) const {
    ConcurrentMap<int, double> matched_index(std::thread::hardware_concurrency());

    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [&](const std::string_view& plus_word){
        const auto& matched_word = index_.find(std::string(plus_word));
        if (matched_word != index_.end()) {
            const double idf = ComputeWordInverseDocumentFreq(std::string(plus_word));
            for (const auto& [id, tf] : index_.at(std::string(plus_word))) {
                const DocumentInfo& document_info = documents_info_.at(id);
                if (filter(id, document_info.status, document_info.rating)) {
                    matched_index[id].ref_to_value += tf * idf;
                }
            }
        }
    });

    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&](const std::string_view& minus_word){
        const auto& matched_word = index_.find(std::string(minus_word));
        if (matched_word != index_.end()) {
            for (const auto& [id, tf] : matched_word->second) {
                matched_index.erase(id);
            }
        }
    });

    std::vector<Document> matched_documents;
    matched_documents.reserve(matched_index.size());
    for (const auto& [id, relevance] : matched_index.BuildOrdinaryMap()) {
        const DocumentInfo& document_info = documents_info_.at(id);
        matched_documents.push_back({ id, relevance, document_info.rating });
    }
    return matched_documents;
}

template<typename T>
SearchServer::SearchServer(const T& stop_words_container) {
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