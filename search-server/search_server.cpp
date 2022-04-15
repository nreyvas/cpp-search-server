#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{}

SearchServer::SearchServer(const std::string_view& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{}

void SearchServer::AddDocument
(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings)
{
    using namespace std::string_literals;
    if (document_id < 0)
        throw std::invalid_argument("Negative ID"s);
    if (documents_.count(document_id) > 0)
        throw std::invalid_argument("ID "s + std::to_string(document_id) + " is already used"s);

    const auto [it, _] = documents_.emplace(document_id,
        DocumentData{ ComputeAverageRating(ratings), std::string(document), status });
    std::vector<std::string_view> words = SplitIntoWordsNoStop(it->second.doc_text);

    const double inv_word_count = 1.0 / words.size();
    for (std::string_view word : words)
    {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const
{
    return FindTopDocuments(raw_query,
        [status](int document_id, DocumentStatus document_status, int rating)
        { return document_status == status; });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequences(int document_id) const
{
    if (document_to_word_freqs_.count(document_id) > 0)
        return document_to_word_freqs_.at(document_id);
    static std::map<std::string_view, double> result;
    return result;
}

void SearchServer::RemoveDocument(int document_id)
{
    if (document_to_word_freqs_.count(document_id) == 0)
        return;

    
    for (const auto& [word, _] : document_to_word_freqs_.at(document_id))
    {
        word_to_document_freqs_[word].erase(document_id);
    }

    document_to_word_freqs_.erase(document_id);

    documents_.erase(document_id);

    const auto it = std::find(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(it);
}

std::vector<int>::const_iterator SearchServer::begin() const
{
    return document_ids_.begin(); 
}

std::vector<int>::const_iterator SearchServer::end() const
{
    return document_ids_.end();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument
(const std::string_view& raw_query, int document_id) const
{
    if (document_to_word_freqs_.count(document_id) == 0)
    {
        throw std::out_of_range("id not found in SearchServer::MatchDocument"s);
    }

    Query query = ParseQuery(raw_query);
    query.SortQuery(std::execution::seq);

    std::vector<std::string_view> matched_words;
    const std::map<std::string_view, double>& doc_ref = document_to_word_freqs_.at(document_id);
    bool bMinusWordsFound = false;

    for (const std::string_view& m_word : query.minus_words)
    {
        if (doc_ref.count(m_word))
        {
            bMinusWordsFound = true;
            break;
        }
    }

    if (!bMinusWordsFound)
    {
        for (std::string_view p_word : query.plus_words)
        {
            if (doc_ref.count(p_word))
            {
                matched_words.push_back(p_word);
            }
        }
    }

    return std::tuple<std::vector<std::string_view>, DocumentStatus>{ matched_words, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument
(std::execution::sequenced_policy, const std::string_view& raw_query, int document_id) const
{
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument
(std::execution::parallel_policy, const std::string_view& raw_query, int document_id) const
{
    if (document_to_word_freqs_.count(document_id) == 0)
    {
        throw std::out_of_range("id not found in SearchServer::MatchDocument"s);
    }

    Query query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;

    const std::map<std::string_view, double>& doc_ref = document_to_word_freqs_.at(document_id);
    bool bMinusWordsFound = false;
    bMinusWordsFound = std::any_of(query.minus_words.begin(), query.minus_words.end(),
        [&](const std::string_view& m_word) { return doc_ref.count(m_word); });

    if (!bMinusWordsFound)
    {

        for (std::string_view p_word : query.plus_words)
        {
            if (doc_ref.count(p_word))
            {
                matched_words.push_back(p_word);
            }
        }

        std::sort(std::execution::par, matched_words.begin(), matched_words.end());
        auto it = std::unique(matched_words.begin(), matched_words.end());
        matched_words.resize(it - matched_words.begin());
    }

    return std::tuple<std::vector<std::string_view>, DocumentStatus>{ matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const std::string_view& word) const
{
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view& word)
{
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text)
{
    std::vector<std::string_view> result;
    for (const std::string_view& word : SplitIntoWords(text))
    {
        if (!IsValidWord(word))
        {
            throw std::invalid_argument("Invalid word: "s + static_cast<std::string>(word));
        }
        if (!IsStopWord(word))
        {
            result.push_back(word);
        }
    }
    return result;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
    if (ratings.empty())
    {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const
{

    if (text.empty())
        throw std::invalid_argument("Empty word"s);

    bool is_stop_word = IsStopWord(text);
    bool is_minus = false;
    if (text[0] == '-')
    {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty())
        throw std::invalid_argument("Empty word"s);
    if (text[0] == '-')
        throw std::invalid_argument("Invalid word: "s + static_cast<std::string>(text));
    if (!IsValidWord(text))
        throw std::invalid_argument("Invalid symbols in word: "s + static_cast<std::string>(text));

    return QueryWord{ std::move(text), is_minus, is_stop_word };
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view& text) const
{
    Query query;
    std::vector<std::string_view> splitted_words = SplitIntoWords(text);
    for (std::string_view& word : splitted_words)
    {
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                query.minus_words.push_back(query_word.data);
            }
            else
            {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view& word) const
{
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}