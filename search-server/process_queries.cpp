#include <execution>
#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries( const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(),
        result.begin(), [&search_server](std::string query)
        {
            return search_server.FindTopDocuments(query);
        });
    return result;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> pre_result(queries.size());
    int size = 0;
    std::transform(std::execution::par, queries.begin(), queries.end(),
        pre_result.begin(), [&search_server, &size](std::string query)
        {
            auto res = search_server.FindTopDocuments(query);
            size += res.size();
            return search_server.FindTopDocuments(query);
        });
    std::vector<Document> result;
    result.reserve(size);
    for (auto& query_results : pre_result)
    {
        for (Document& it_result : query_results)
        {
            result.push_back(std::move(it_result));
        }
    }
    return result;
}