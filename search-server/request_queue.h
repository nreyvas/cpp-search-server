#pragma once
#include <queue>
#include <vector>
#include <string>
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : server(search_server) {}

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate)
    {
        auto search_results = server.FindTopDocuments(raw_query, document_predicate);
        RegisterRequest(search_results.empty());
        return search_results;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const { return no_result_count_; }
private:
    struct QueryResult
    {
        bool no_results;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& server;
    int minute_count_ = 0;
    int no_result_count_ = 0;

    void RegisterRequest(bool no_results);
};