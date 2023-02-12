template<typename TFilter>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, TFilter filter) const {
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
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, TFilter filter) const {
    std::map<int, double> matched_index;
    for (const std::string& plus_word : query.words) {
        if (index_.find(plus_word) != index_.end()) {
            const double idf = ComputeWordInverseDocumentFreq(plus_word);
            for (const auto& [id, tf] : index_.at(plus_word)) {
                matched_index[id] += tf * idf;
            }
        }
    }

    for (const std::string& minus_word : query.minus_words) {
        if (index_.find(minus_word) != index_.end()) {
            for (const auto& [id, tf] : index_.at(minus_word)) {
                matched_index.erase(id);
            }
        }
    }

    std::vector<Document> matched_documents;
    for (const auto& [id, relevance] : matched_index) {
        const DocumentInfo& document_info = documents_info_.at(id);
        if (filter(id, document_info.status, document_info.rating)) {
            matched_documents.push_back({ id, relevance, document_info.rating });
        }
    }
    return matched_documents;
}