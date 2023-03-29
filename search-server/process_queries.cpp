#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer &search_server, const std::vector<std::string> &queries) {
    std::vector<std::vector<Document>> result(queries.size());
    result.reserve(queries.size());

    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const std::string query){
        return search_server.FindTopDocuments(query);
    });

    return result;
}

std::list<Document> ProcessQueriesJoined(const SearchServer &search_server, const std::vector<std::string> &queries) {
    std::vector<std::vector<Document>> processed_queries = ProcessQueries(search_server, queries);

    std::list<Document> result;
    //std::execution::par работает при тестах с малым количеством, но не проходит тренажер.
    std::for_each(std::execution::seq, processed_queries.begin(), processed_queries.end(), [&result](std::vector<Document>& item){
        std::list<Document> documents(std::make_move_iterator(item.begin()), std::make_move_iterator(item.end()));
        result.splice(result.end(), documents);
    });

    return result;
}
