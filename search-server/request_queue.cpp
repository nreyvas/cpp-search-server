#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server)
    : server(search_server) {}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status)
{
    auto search_results = server.FindTopDocuments(raw_query, status);
    RegisterRequest(search_results.empty());
    return search_results;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query)
{
    auto search_results = server.FindTopDocuments(raw_query);
    RegisterRequest(search_results.empty());
    return search_results;
}

int RequestQueue::GetNoResultRequests() const
{
    return no_result_count_; 
}

void RequestQueue::RegisterRequest(bool no_results)
{
    if (minute_count_ <= min_in_day_)
        ++minute_count_;
    if (minute_count_ > min_in_day_)
    {
        if (requests_.front().no_results)
            --no_result_count_;
        requests_.pop_front();
    }
    requests_.push_back(QueryResult{ no_results });
    if (no_results)
        ++no_result_count_;
}
