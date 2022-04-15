#pragma once
#include <vector>
#include <string>
#include <set>
#include <map>
#include <numeric>
#include <algorithm>
#include <math.h>
#include <execution>
#include <type_traits>
#include "string_processing.h"
#include <type_traits>

#include "document.h"
#include "concurrent_map.h"

#include "log_duration.h"

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer
{
public:

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);

    explicit SearchServer(const std::string_view& stop_words_text);

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query) const;

    int GetDocumentCount() const { return documents_.size(); }

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument
        (std::execution::sequenced_policy, const std::string_view& raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument
        (std::execution::parallel_policy, const std::string_view& raw_query, int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequences(int document_id) const;

    void RemoveDocument(int document_id);

    template <class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);

    std::vector<int>::const_iterator begin() const;

    std::vector<int>::const_iterator end() const;

private:

    struct DocumentData
    {
        int rating;
        std::string doc_text;
        DocumentStatus status;
    };

    struct QueryWord
    {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct Query
    {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;

        bool plus_words_sorted = false;
        bool minus_words_sorted = false;

        template <class ExecutionPolicy>
        void SortQuery(ExecutionPolicy&& policy);
    };

    const std::set<std::string,std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_; // Document ID and Data (rating, status)
    std::vector<int> document_ids_;

    bool IsStopWord(const std::string_view& word) const;
    static bool IsValidWord(const std::string_view& word);
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text);
    static int ComputeAverageRating(const std::vector<int>& ratings);

    QueryWord ParseQueryWord(std::string_view text) const;

    Query ParseQuery(const std::string_view& text) const;

    template <class ExecutionPolicy>
    Query ParseQuery(ExecutionPolicy&& policy, const std::string& text) const;

    double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments
        (std::execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments
        (std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const;
};

//--------------------------------------TEMPLATE----METHODS-----------------------------------------------

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    for (const std::string& word : stop_words_)
        if (!IsValidWord(word))
            throw std::invalid_argument("Invalid stop word: "s + word);
}

template <class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id)
{
    if (document_to_word_freqs_.count(document_id) == 0)
        return;
    std::map<std::string, double>& doc_ref = document_to_word_freqs_.at(document_id);

    std::vector<const std::string*> doc_words;
    doc_words.reserve(doc_ref.size());
    for (auto& p : doc_ref)
    {
        doc_words.push_back(&(p.first));
    }
    std::for_each(policy, doc_words.begin(), doc_words.end(),
        [&](const std::string* word_ptr) { word_to_document_freqs_.at(*word_ptr).erase(document_id); });

    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);

    const auto it_doc = std::find(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(it_doc);
}

template <class ExecutionPolicy>
void SearchServer::Query::SortQuery(ExecutionPolicy&& policy)
{
    if (!plus_words_sorted)
    {
        std::sort(policy, plus_words.begin(), plus_words.end());
        auto erase_from = std::unique(plus_words.begin(), plus_words.end());
        plus_words.erase(erase_from, plus_words.end());
        plus_words_sorted = true;
    }
    if (!minus_words_sorted)
    {
        std::sort(policy, minus_words.begin(), minus_words.end());
        auto erase_from = std::unique(minus_words.begin(), minus_words.end());
        minus_words.erase(erase_from, minus_words.end());
        minus_words_sorted = true;
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const
{
    std::map<int, double> document_to_relevance;
    for (const std::string_view& word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
        {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating))
            {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view& word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word))
        {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance)
    {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments                                                                // TODO
    (std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const
{
    ConcurrentMap<int, double> document_to_relevance(std::thread::hardware_concurrency());

    // parsing plus words    
    ForEach(std::execution::par, query.plus_words,
        [this, &document_to_relevance, &document_predicate](const std::string_view& word)
        {
            if (word_to_document_freqs_.count(word) != 0)
            {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
                {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating))
                    {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        });

    // parsing minus words
    ForEach(std::execution::par, query.minus_words,
        [this, &document_to_relevance](const std::string_view& word)
        {
            if (word_to_document_freqs_.count(word) != 0)
            {
                for (const auto [document_id, _] : word_to_document_freqs_.at(word))
                {
                    document_to_relevance[document_id].ref_to_value = 0;
                }
            }
        });

    // generating ordinary map
    std::map<int, double> ordinary_map = document_to_relevance.BuildOrdinaryMap();

    // copying it to result
    std::vector<Document> matched_documents(document_to_relevance.Size());
    std::transform(std::execution::par, ordinary_map.begin(), ordinary_map.end(), matched_documents.begin(),
        [this](std::pair<const int, double> pair)
        { return Document{ pair.first, pair.second, documents_.at(pair.first).rating }; });

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const
{
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments                                                        // TODO
(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const
{
    if (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>)
    {
        return FindTopDocuments(raw_query, document_predicate);
    }
    
    // parsing query
    Query query = ParseQuery(std::move(raw_query));
    query.SortQuery(std::execution::par);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    // sortig documents
    sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs)                                      
        {
            if (abs(lhs.relevance - rhs.relevance) < EPSILON)
            {
                return lhs.rating > rhs.rating;
            }
            else { return lhs.relevance > rhs.relevance; }
        });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
    {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);

    }
    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments
    (ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const
{
    return FindTopDocuments(policy, raw_query,
        [status](int document_id, DocumentStatus document_status, int rating)
        { return document_status == status; });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query) const
{
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}