#include "search_server.h"

void SearchServer::SetStopWords(const std::string& text) {
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Contains special symbols");
        }
        stop_words_.insert(word);
    }
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    using namespace std::literals::string_literals;
    if (document_id < 0) throw std::invalid_argument("Negative ID"s);
    if (documents_info_.count(document_id)) throw std::invalid_argument("This ID already exists"s);

    const std::vector<std::string> words = SplitIntoWordsNoStop(document);
    const double tf_one_word = 1.0 / words.size();
    for (const std::string& word : words) {
        index_[word][document_id] += tf_one_word;
    }
    documents_info_.emplace(document_id, DocumentInfo{ ComputeAverageRating(ratings), status });
    document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, const DocumentStatus& status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return status == document_status; });
}

int SearchServer::GetDocumentCount() const {
    return documents_info_.size();
}

std::set<std::string> SearchServer::GetStopWords() const {
    return stop_words_;
}

int SearchServer::GetDocumentId(int index) const {
    return document_ids_.at(index);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    Query query = ParseQuery(raw_query);
    std::set<std::string> matched_plus_words;
    bool contains_minus_word = false;

    for (const std::string& minus_word : query.minus_words) {
        if (index_.find(minus_word) != index_.end() && index_.at(minus_word).count(document_id)) {
            contains_minus_word = true;
        }
    }

    if (!contains_minus_word)
    {
        for (const std::string& plus_word : query.words) {
            if (index_.find(plus_word) != index_.end() && index_.at(plus_word).count(document_id)) {
                matched_plus_words.emplace(plus_word);
            }
        }
    }
    std::vector<std::string> plus_words_vector(matched_plus_words.begin(), matched_plus_words.end());
    return { plus_words_vector, documents_info_.at(document_id).status };
}

SearchServer::SearchServer() = default;
SearchServer::SearchServer(const std::string& stop_words) {
    SetStopWords(stop_words);
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsMinusWord(const std::string& word) const {
    if (word.at(0) == '-') {
        if (word.size() <= 1 || word.at(1) == '-') {
            throw std::invalid_argument("two minuses or nothing after minus");
        }
        return true;
    }
    return false;
}

std::vector<SearchServer::WordInfo> SearchServer::ParseWords(const std::string& text) const {
    std::vector<SearchServer::WordInfo> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Contains special symbols");
        }
        if (IsStopWord(word)) {
            words.push_back({ word, false, false, true });
        }
        else if (IsMinusWord(word)) {
            words.push_back({ word, false, true });
        }
        else {
            words.push_back({ word, true });
        }
    }
    return words;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const WordInfo& word : ParseWords(text)) {
        if (!word.is_stop_word) {
            words.push_back(word.word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (!ratings.empty()) {
        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    }
    return 0;
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    for (const std::string& word : SplitIntoWordsNoStop(text)) {
        if (word[0] == '-') {
            if (word.size() <= 1 || word[1] == '-') {
                throw std::invalid_argument("two minuses or nothing after minus");
            }
            query.minus_words.insert(word.substr(1));
        }
        else {
            query.words.insert(word);
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return std::log(static_cast<double>(documents_info_.size()) / index_.at(word).size());
}

bool SearchServer::IsValidWord(const std::string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}