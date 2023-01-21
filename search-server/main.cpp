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
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0) throw invalid_argument("Negative ID"s);
        if (documents_info_.count(document_id)) throw invalid_argument("This ID already exists"s);

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double tf_one_word = 1.0 / words.size();
        for (const string& word : words) {
            index_[word][document_id] += tf_one_word;
        }
        documents_info_.emplace(document_id, DocumentInfo{ComputeAverageRating(ratings), status});
        document_ids_.push_back(document_id);
    }

    template<typename TFilter>
    vector<Document> FindTopDocuments(const string& raw_query, TFilter filter) const {
        Query query = ParseQuery(raw_query);

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

    vector<Document> FindTopDocuments(const string& raw_query, const DocumentStatus& status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return status == document_status; });
    }

    int GetDocumentCount() const {
        return documents_info_.size();
    }

    set<string> GetStopWords() const {
        return stop_words_;
    }

    int GetDocumentId(int index) const {
        return document_ids_.at(index);
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        Query query = ParseQuery(raw_query);
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
        return { plus_words_vector, documents_info_.at(document_id).status };
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
                if (!IsValidWord(stop_word)) {
                    throw invalid_argument("Contains special symbols");
                }
                stop_words_.insert(stop_word);
            }
        }
    }

private:
    struct DocumentInfo;
    set<string> stop_words_;
    map<string, map<int, double>> index_;
    map<int, DocumentInfo> documents_info_;
    vector<int> document_ids_;

    struct WordInfo
    {
        string word = "";
        bool is_plus_word = false;
        bool is_minus_word = false;
        bool is_stop_word = false;
    };

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    bool IsMinusWord(const string& word) const {
        if (word.at(0) == '-') {
            if (word.size() <= 1 || word.at(1) == '-') {
                throw invalid_argument("two minuses or nothing after minus");
            }
            return true;
        }
        return false;
    }

    vector<string> SplitIntoWords(const string& text) const {
        vector<string> words;
        string word;
        for (const char c : text) {
            if (c == ' ') {
                if (!word.empty()) {
                    if (!IsValidWord(word)) {
                        throw invalid_argument("Contains special symbols");
                    }
                    words.push_back(word);
                    word.clear();
                }
            }
            else {
                word += c;
            }
        }

        if (!word.empty()) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Contains special symbols");
            }
            words.push_back(word);
        }

        return words;
    }

    vector<WordInfo> ParseWords(const string& text) const {
        vector<WordInfo> words;
        for (const string& word : SplitIntoWords(text)) {
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

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const WordInfo& word : ParseWords(text)) {
            if (!word.is_stop_word) {
                words.push_back(word.word);
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
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "возвращен не пустой список.");
    }
}

// Тест проверяет, что документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
void TestExcludeDocumentsWithMinusWords() {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "пушистый ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    for (const Document& document : search_server.FindTopDocuments("пушистый кот -хвост"s)) {
        ASSERT(document.id != 1);
    }
}

// Тест проверяет, что при матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса,
// присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestDocumentMatching() {
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        auto [matched_words, status] = search_server.MatchDocument("модный белый кот"s, 0);
        ASSERT_EQUAL(matched_words.size(), 3);
    }

    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        auto [matched_words, status] = search_server.MatchDocument("модный белый -кот"s, 0);
        ASSERT(matched_words.empty());
    }
}

void TestSortingByRelevancy() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT(result.at(0).relevance > result.at(1).relevance && result.at(1).relevance > result.at(2).relevance);
}

void TestRelevancyCalc() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    constexpr double EPSILON = 1e-6;
    ASSERT(result.at(0).relevance - 0.866434 < EPSILON);
}

void TestRatingCalc() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_EQUAL(result.at(0).rating, 5);
}

void TestSearchByStatus() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(result.at(0).id, 3);
}

void TestUserPredicate() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    auto result = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
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

template <typename Iter>
void PrintRange(Iter range_begin, Iter range_end) {
    for (auto it = range_begin; it != range_end; ++it) {
        std::cout << *it;
    }
}

template <typename It>
class IteratorRange {
public:
    It begin() const {
        return _begin;
    }

    It end() const {
        return _end;
    }

    [[nodiscard]]
    int size() const {
        return distance(_begin, _end);
    }

    IteratorRange(It begin, It end) {
        _begin = begin;
        _end = end;
    }

private:
    It _begin;
    It _end;
};

template <typename It>
ostream& operator<<(ostream& os, const IteratorRange<It>& range)
{
    PrintRange(range.begin(), range.end());
    return os;
}

ostream& operator<<(ostream& os, const Document& document) {
    os << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s;
    return os;
}

template <typename It>
class Paginator {
public:
    Paginator(It begin, It end, size_t page_size) {
        while (distance(begin, end) > 0 && distance(begin, end) >= page_size)
        {
            auto range_begin = begin;
            advance(begin, page_size);
            auto range_end = begin;
            _pages.push_back({ range_begin, range_end });
        }
        if (begin != end) _pages.push_back({ begin, end });
    }

    auto begin() const {
        return _pages.begin();
    }

    auto end() const {
        return _pages.end();
    }

private:
    vector<IteratorRange<It>> _pages;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
int main() {
    TestSearchServer();

    SearchServer search_server("and with"s);
    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, { 1, 1, 1 });
    const auto search_results = search_server.FindTopDocuments("curly dog"s);
    int page_size = 1;
    const auto pages = Paginate(search_results, page_size);
    cout << "Paginated" << endl;
    // Выводим найденные документы по страницам
    for (auto page = pages.begin(); page != pages.end(); ++page) {
        cout << *page << endl;
        cout << "Page break"s << endl;
    }
}

//int main() {
//    TestSearchServer();
//
//    SearchServer search_server;
//    search_server.SetStopWords("и в на"s);
//    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
//    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
//    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
//    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
//    cout << "ACTUAL by default:"s << endl;
//    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
//        PrintDocument(document);
//    }
//    cout << "BANNED:"s << endl;
//    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
//        PrintDocument(document);
//    }
//    cout << "Even ids:"s << endl;
//    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
//        PrintDocument(document);
//    }
//    return 0;
//}