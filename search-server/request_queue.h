#pragma once
#include <vector>
#include <string>
#include <deque>
#include <algorithm>
#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        const auto search_results = search_server_->FindTopDocuments(raw_query, document_predicate);
        AddToRequests(raw_query, search_results.size());
        return search_results;
    }
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;

private:
    void AddToRequests(const std::string& raw_query, size_t number_of_results);

    struct QueryResult {
        std::string raw_query;
        size_t number_of_results;
    };

    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;

    const SearchServer* search_server_;
};