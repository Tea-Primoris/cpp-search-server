#include "search_server.h"

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    using namespace std::literals::string_literals;
    if (document_id < 0) throw std::invalid_argument("Negative ID"s);
    if (documents_info_.count(document_id)) throw std::invalid_argument("This ID already exists"s);

    const std::vector<std::string_view> words = SplitIntoWordsNoStop(document);
    const double tf_one_word = 1.0 / words.size();
    std::map<std::string, double> freqs_of_words;

    std::vector<std::string> document_words(words.begin(), words.end());
    document_words.erase(std::unique(document_words.begin(), document_words.end()), document_words.end());
    std::sort(document_words.begin(), document_words.end());

    for (const std::string_view word : words) {
        index_[std::string(word)][document_id] += tf_one_word;
        freqs_of_words[std::string(word)] += tf_one_word;
    }
    documents_info_.emplace(document_id, DocumentInfo{ ComputeAverageRating(ratings), status, freqs_of_words, document_words });
    document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, const DocumentStatus& status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return status == document_status; });
}

int SearchServer::GetDocumentCount() const {
    return documents_info_.size();
}

std::set<std::string> SearchServer::GetStopWords() const {
    return stop_words_;
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    const std::map<std::string, double>* freqs;
    try
    {
        freqs = &documents_info_.at(document_id).freqs_of_words;
    }
    catch (const std::out_of_range&)
    {
        using namespace std::string_literals;
        std::cerr << "No such ID"s << std::endl;
        throw;
    }
    return *freqs;
}

void SearchServer::RemoveDocument(int document_id) {
    document_ids_.erase(document_id);
    const std::map<std::string, double>* words;
    try
    {
        words = &documents_info_.at(document_id).freqs_of_words;
    }
    catch (const std::out_of_range&)
    {
        using namespace std::string_literals;
        std::cerr << "No such ID"s << std::endl;
        return;
    }
    for (auto iterator = words->begin(); iterator != words->end(); iterator = std::next(iterator)) {
        std::string word = iterator->first;
        index_[word].erase(document_id);
    }
    documents_info_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id) {
    const std::vector<std::string>* document_content;
    try
    {
        document_content = &documents_info_.at(document_id).content;
    }
    catch (const std::out_of_range&)
    {
        using namespace std::string_literals;
        std::cerr << "No such ID"s << std::endl;
        return;
    }

    std::vector<const std::string*> document_words(document_content->size());
    std::transform(std::execution::par, document_content->begin(), document_content->end(), document_words.begin(), [](const auto& item){
        return &item;
    });

    std::for_each(std::execution::par, document_words.begin(), document_words.end(), [this, &document_id](const std::string*& word){
        index_[*word].erase(document_id);
    });

    document_ids_.erase(document_id);
    documents_info_.erase(document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    //LOG_DURATION_STREAM("Operation time", std::cout);
    Query query = ParseQuery(raw_query);
    std::set<std::string_view> matched_plus_words;
    bool contains_minus_word = false;

    for (const std::string_view minus_word : query.minus_words) {
        if (index_.find(std::string(minus_word)) != index_.end() && index_.at(std::string(minus_word)).count(document_id)) {
            contains_minus_word = true;
        }
    }

    if (!contains_minus_word)
    {
        for (const std::string_view plus_word : query.plus_words) {
            if (index_.find(std::string(plus_word)) != index_.end() && index_.at(std::string(plus_word)).count(document_id)) {
                matched_plus_words.emplace(plus_word);
            }
        }
    }
    std::vector<std::string_view> plus_words_vector(matched_plus_words.begin(), matched_plus_words.end());
    return { plus_words_vector, documents_info_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const {
    Query query = ParseQuery(raw_query, false);

    const std::vector<std::string>* document_content;
    try {
        document_content = &documents_info_.at(document_id).content;
    }
    catch (std::out_of_range&) {
        return { std::vector<std::string_view>(), DocumentStatus::REMOVED};
    }

    const bool contains_minus_words = std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&document_content](const std::string_view minus_word){
        return std::find(document_content->begin(), document_content->end(), minus_word) != document_content->end();
    });

    std::vector<std::string_view> matched_plus_words;

    if(!contains_minus_words) {
        matched_plus_words.reserve(query.plus_words.size());
        std::copy_if(std::execution::seq, query.plus_words.begin(), query.plus_words.end(), std::back_inserter(matched_plus_words), [&document_content](const std::string_view plus_word){
            return std::find(document_content->begin(), document_content->end(), std::string(plus_word)) != document_content->end();
        });
        std::sort(std::execution::par, matched_plus_words.begin(), matched_plus_words.end());
        matched_plus_words.erase(std::unique(std::execution::par, matched_plus_words.begin(), matched_plus_words.end()), matched_plus_words.end());
    }

    return { matched_plus_words, documents_info_.at(document_id).status };
}

auto SearchServer::begin() const -> std::set<int>::const_iterator {
    return document_ids_.begin();
}

auto SearchServer::end() const -> std::set<int>::const_iterator {
    return document_ids_.end();
}

SearchServer::SearchServer() = default;

SearchServer::SearchServer(const std::string& stop_words) {
    for (const std::string& word : SplitIntoWords(stop_words)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Contains special symbols");
        }
        stop_words_.insert(word);
    }
}

SearchServer::SearchServer(const std::string_view stop_words) {
    for (const std::string_view word : SplitIntoWords(stop_words)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Contains special symbols");
        }
        stop_words_.insert(std::string(word));
    }
}

bool SearchServer::IsStopWord(const std::string_view word) const {
    return stop_words_.count(std::string(word));
}

bool SearchServer::IsMinusWord(const std::string_view word) const {
    if (word.at(0) == '-') {
        if (word.size() <= 1 || word.at(1) == '-') {
            throw std::invalid_argument("two minuses or nothing after minus");
        }
        return true;
    }
    return false;
}

std::vector<SearchServer::WordInfo> SearchServer::ParseWords(const std::string_view text) const {
    std::vector<SearchServer::WordInfo> words;
    for (const std::string_view word : SplitIntoWords(text)) {
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

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;
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

SearchServer::Query SearchServer::ParseQuery(const std::string_view text, bool sort_results) const {
    Query query;
    for (const std::string_view word : SplitIntoWordsNoStop(text)) {
        if (word[0] == '-') {
            if (word.size() <= 1 || word[1] == '-') {
                throw std::invalid_argument("two minuses or nothing after minus");
            }
            query.minus_words.push_back(word.substr(1));
        }
        else {
            query.plus_words.push_back(word);
        }
    }

    if (sort_results) {
        std::sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(std::unique(query.plus_words.begin(), query.plus_words.end()), query.plus_words.end());

        std::sort(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(std::unique(query.minus_words.begin(), query.minus_words.end()), query.minus_words.end());
    }

    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return std::log(static_cast<double>(documents_info_.size()) / index_.at(word).size());
}

bool SearchServer::IsValidWord(const std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

auto SearchServer::documents_info_begin() -> std::map<int, DocumentInfo>::iterator {
    return documents_info_.begin();
}

auto SearchServer::documents_info_end() -> std::map<int, DocumentInfo>::iterator {
    return documents_info_.end();
}