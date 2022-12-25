#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <cassert>
#include <optional>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

ostream& operator<<(ostream& os, const set<string>& set)
{
    for (const string& item : set) {
        os << item;
    }
    return os;
}

template <typename T>
void RunTestImpl(T func, string func_name) {
    func();
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

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
    int id = 0;
    double relevance = 0.0;
    int rating = 0;

    Document() = default;

    Document(const int doc_id, double doc_relevancy, int doc_rating)
        : id(doc_id), relevance(doc_relevancy), rating(doc_rating)
    {

    }
};

class SearchServer {
public:

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Contains special symbols");
            }
            stop_words_.insert(word);
        }
    }

    [[nodiscard]] bool AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0 || documents_info_.count(document_id) || !IsValidWord(document)) return false;

        vector<string> words;
        try {
            words = SplitIntoWordsNoStop(document);
        }
        catch (const std::invalid_argument& e) {
            return false;
        }

        const double tf_one_word = 1.0 / words.size();
        for (const string& word : words) {
            index_[word][document_id] += tf_one_word;
        }
        documents_info_.emplace(document_id, DocumentInfo{ComputeAverageRating(ratings), status});
        return true;
    }

    template<typename TFilter>
    optional<vector<Document>> FindTopDocuments(const string& raw_query, TFilter filter) const {
        Query query;
        try
        {
            query = ParseQuery(raw_query);
        }
        catch (const std::invalid_argument& e)
        {
            return nullopt;
        }

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

    optional<vector<Document>> FindTopDocuments(const string& raw_query, const DocumentStatus& status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return status == document_status; });
    }

    int GetDocumentCount() const {
        return documents_info_.size();
    }

    set<string> GetStopWords() const {
        return stop_words_;
    }

    int GetDocumentId(int index) const {
        int counter = 0;
        for (const auto&[id, document_info] : documents_info_)
        {
            if (index == counter)
            {
                return id;
            }
            ++counter;
        }
        throw out_of_range("Out of range");
    }

    optional<tuple<vector<string>, DocumentStatus>> MatchDocument(const string& raw_query, int document_id) const {
        Query query;
        try
        {
            query = ParseQuery(raw_query);
        }
        catch (const std::invalid_argument& e)
        {
            return nullopt;
        }
        
        set<string> matched_plus_words;
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
        tuple<vector<string>, DocumentStatus> return_value = { plus_words_vector, documents_info_.at(document_id).status };
        return return_value;
    }

    SearchServer() = default;

    explicit SearchServer(const string& stop_words) {
        SetStopWords(stop_words);
    }

    template<typename T>
    SearchServer(const T& stop_words_container) {
        for (const string stop_word : stop_words_container) {
            if (!stop_word.empty())
            {
                stop_words_.insert(stop_word);
            }
        }
    }

private:
    struct DocumentInfo;
    set<string> stop_words_;
    map<string, map<int, double>> index_;
    map<int, DocumentInfo> documents_info_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Contains special symbols");
            }
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (!ratings.empty()) {
            return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
        }
        return 0;
    }

    struct Query {
        set<string> words;
        set<string> minus_words;
    };

    struct DocumentInfo {
        int rating;
        DocumentStatus status;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-') {
                if (word.size() <= 1 || word[1] == '-') {
                    throw invalid_argument("two minuses or nothing after minus");
                }
                query.minus_words.insert(word.substr(1));
            }
            else {
                query.words.insert(word);
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(static_cast<double>(documents_info_.size()) / index_.at(word).size());
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
            const DocumentInfo& document_info = documents_info_.at(id);
            if (filter(id, document_info.status, document_info.rating)) {
                matched_documents.push_back({ id, relevance, document_info.rating });
            }
        }
        return matched_documents;
    }

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        ignore = server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = *server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        ignore = server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).value().empty(), "возвращен не пустой список.");
    }
}

// Тест проверяет, что документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
void TestExcludeDocumentsWithMinusWords() {
    SearchServer search_server;
    ignore = search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    ignore = search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    ignore = search_server.AddDocument(2, "пушистый ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    for (const Document& document : *search_server.FindTopDocuments("пушистый кот -хвост"s)) {
        ASSERT(document.id != 1);
    }
}

// Тест проверяет, что при матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса,
// присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestDocumentMatching() {
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        ignore = search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        auto [matched_words, status] = *search_server.MatchDocument("модный белый кот"s, 0);
        ASSERT_EQUAL(matched_words.size(), 3);
    }

    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        ignore = search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        auto [matched_words, status] = *search_server.MatchDocument("модный белый -кот"s, 0);
        ASSERT(matched_words.empty());
    }
}

void TestSortingByRelevancy() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    ignore = search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    ignore = search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    ignore = search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    auto result = *search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT(result.at(0).relevance > result.at(1).relevance && result.at(1).relevance > result.at(2).relevance);
}

void TestRelevancyCalc() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    ignore = search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    auto result = *search_server.FindTopDocuments("пушистый ухоженный кот"s);
    constexpr double EPSILON = 1e-6;
    ASSERT(result.at(0).relevance - 0.866434 < EPSILON);
}

void TestRatingCalc() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    ignore = search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    auto result = *search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_EQUAL(result.at(0).rating, 5);
}

void TestSearchByStatus() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    ignore = search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    ignore = search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    auto result = *search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(result.at(0).id, 3);
}

void TestUserPredicate() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    ignore = search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    ignore = search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    ignore = search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    ignore = search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    auto result = *search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT(result.at(0).id == 0 && result.at(1).id == 2);
}

void TestConstructorWithStopWordsString() {
    {
        SearchServer search_server("и в на"s);
        set<string> expected_stop_words{ "и"s, "в"s, "на"s };
        ASSERT_EQUAL(expected_stop_words, search_server.GetStopWords());
    }

    {
        SearchServer search_server("  и в   на  "s);
        set<string> expected_stop_words{ "и"s, "в"s, "на"s };
        ASSERT_EQUAL(expected_stop_words, search_server.GetStopWords());
    }

    {
        SearchServer search_server("и   в   на     "s);
        set<string> expected_stop_words{ "и"s, "в"s, "на"s };
        ASSERT_EQUAL(expected_stop_words, search_server.GetStopWords());
    }
}

void TestConstructorWithStopWordsContainer() {
    {
        set<string> expected_stop_words{ "и"s, "в"s, "на"s };
        SearchServer search_server(expected_stop_words);
        ASSERT_EQUAL(expected_stop_words, search_server.GetStopWords());
    }

    {
        vector<string> stop_words{ "и"s, "в"s, "на"s, "на"s, "и"s };
        set<string> expected_stop_words{ "и"s, "в"s, "на"s };
        SearchServer search_server("и в на"s);
        ASSERT_EQUAL(expected_stop_words, search_server.GetStopWords());
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsWithMinusWords);
    RUN_TEST(TestDocumentMatching);
    RUN_TEST(TestSortingByRelevancy);
    RUN_TEST(TestRelevancyCalc);
    RUN_TEST(TestRatingCalc);
    RUN_TEST(TestSearchByStatus);
    RUN_TEST(TestUserPredicate);
    RUN_TEST(TestConstructorWithStopWordsString);
    RUN_TEST(TestConstructorWithStopWordsContainer);
}

// --------- Окончание модульных тестов поисковой системы -----------



int main() {
    TestSearchServer();

    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    ignore = search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    ignore = search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    ignore = search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    ignore = search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    cout << "ACTUAL by default:"s << endl;
    const auto documents = *search_server.FindTopDocuments("пушистый ухоженный кот"s);
    for (const Document& document : documents) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    const auto banned_documents = *search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
    for (const Document& document : banned_documents) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    const auto documents_with_predicate = *search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    for (const Document& document : documents_with_predicate) {
        PrintDocument(document);
    }
    return 0;
}