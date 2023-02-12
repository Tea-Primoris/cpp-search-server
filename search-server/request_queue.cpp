#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) {
    search_server_ = &search_server;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    const auto search_results = search_server_->FindTopDocuments(raw_query, status);
    AddToRequests(raw_query, search_results.size());
    return search_results;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    const auto search_results = search_server_->FindTopDocuments(raw_query);
    AddToRequests(raw_query, search_results.size());
    return search_results;
}

int RequestQueue::GetNoResultRequests() const {
    return count_if(requests_.begin(), requests_.end(), [](QueryResult result) {
        return result.number_of_results == 0;
        });
}

void RequestQueue::AddToRequests(const std::string& raw_query, size_t number_of_results) {
    requests_.push_back(QueryResult{ raw_query, number_of_results });
    if (requests_.size() > min_in_day_) {
        requests_.pop_front();
    }
}