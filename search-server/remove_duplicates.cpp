#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    auto CompareMapsByKey = [](const std::map<std::string, double>& map1, const std::map<std::string, double>& map2) {
        if (map1.size() == map2.size()) {
            for (auto [key, value] : map1) {
                if (!map2.count(key)) {
                    return false;
                }
            }
            return true;
        }
        return false;
    };

    std::set<int> documents_to_delete;
    for (auto iterator = search_server.documents_info_.begin(); iterator != search_server.documents_info_.end(); iterator = std::next(iterator)) {
        const std::map<std::string, double>& words = iterator->second.freqs_of_words;
        for (auto dup_iterator = std::next(iterator); dup_iterator != search_server.documents_info_.end(); dup_iterator = std::next(dup_iterator)) {
            const std::map<std::string, double>& dup_words = dup_iterator->second.freqs_of_words;
            if (CompareMapsByKey(words, dup_words)) {
                int document_id = dup_iterator->first;
                documents_to_delete.insert(document_id);
                {
                    using namespace std::string_literals;
                    std::cout << "Found duplicate document id "s << document_id << std::endl;
                }
            }
        }
    }

    for (int document_id : documents_to_delete) {
        search_server.RemoveDocument(document_id);
    }
}