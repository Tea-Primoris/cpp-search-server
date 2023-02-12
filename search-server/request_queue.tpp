template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    const auto search_results = search_server_->FindTopDocuments(raw_query, document_predicate);
    AddToRequests(raw_query, search_results.size());
    return search_results;
}