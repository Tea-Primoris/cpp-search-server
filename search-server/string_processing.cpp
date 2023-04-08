#include "string_processing.h"

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
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

std::vector<std::string_view> SplitIntoWords(const std::string_view& text) {
    std::vector<std::string_view> words;

    auto word_begin = text.find_first_not_of(' ');
    while (word_begin != std::string_view::npos) {
        auto word_end = text.find_first_of(' ', word_begin);
        words.push_back(text.substr(word_begin, (word_end != std::string_view::npos) ? word_end - word_begin : text.size() - word_end));
        word_begin = text.find_first_not_of(' ', word_end);
    }

    return words;
}