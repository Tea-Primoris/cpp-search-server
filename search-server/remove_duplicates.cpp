#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> documents_to_delete;
    for (auto iterator = search_server.documents_info_begin(); iterator != search_server.documents_info_end(); iterator = std::next(iterator)) {
        const std::vector<std::string>& words = iterator->second.content;
        for (auto dup_iterator = std::next(iterator); dup_iterator != search_server.documents_info_end(); dup_iterator = std::next(dup_iterator)) {
            const std::vector<std::string>& dup_words = dup_iterator->second.content;
            if (words == dup_words) {
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