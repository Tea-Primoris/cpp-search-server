#include "document.h"

Document::Document() = default;

Document::Document(const int doc_id, double doc_relevancy, int doc_rating)
    : id(doc_id), relevance(doc_relevancy), rating(doc_rating) { }

void PrintDocument(const Document& document) {
    using namespace std::literals::string_literals;
    std::cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << std::endl;
}

std::ostream& operator<<(std::ostream& os, const Document& document) {
    using namespace std::literals::string_literals;
    os << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s;
    return os;
}