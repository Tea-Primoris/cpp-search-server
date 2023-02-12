#pragma once
#include <iostream>

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

    Document();
    Document(const int doc_id, double doc_relevancy, int doc_rating);
};

void PrintDocument(const Document& document);
std::ostream& operator<<(std::ostream& os, const Document& document);