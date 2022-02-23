#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{}

void SearchServer::AddDocument
(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings)
{
    using namespace std::string_literals;
    if (document_id < 0)
        throw std::invalid_argument("Negative ID"s);
    if (documents_.count(document_id) > 0)
        throw std::invalid_argument("ID "s + std::to_string(document_id) + " is already used"s);

    std::vector<std::string> words;
    SplitIntoWordsNoStop(document, words);

    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words)
    {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const
{
    return FindTopDocuments(raw_query,
        [status](int document_id, DocumentStatus document_status, int rating)
        { return document_status == status; });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument
(const std::string& raw_query, int document_id) const
{
    Query query = ParseQuery(raw_query);

    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            matched_words.clear();
            break;
        }
    }

    return std::tuple<std::vector<std::string>, DocumentStatus>{ matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const std::string& word) const
{
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string& word)
{
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

bool SearchServer::SplitIntoWordsNoStop(const std::string& text, std::vector<std::string>& result) const
{
    result.clear();
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text))
    {
        if (!IsValidWord(word))
        {
            throw std::invalid_argument("Invalid word: "s + word);
        }
        if (!IsStopWord(word))
        {
            words.push_back(word);
        }
    }
    result.swap(words);
    return true;
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

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const
{

    if (text.empty())
        throw std::invalid_argument("Empty word"s);

    bool is_minus = false;
    if (text[0] == '-')
    {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty())
        throw std::invalid_argument("Empty word"s);
    if (text[0] == '-')
        throw std::invalid_argument("Invalid word: "s + text);
    if (!IsValidWord(text))
        throw std::invalid_argument("Invalid symbols in word: "s + text);

    return QueryWord{ text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const
{
    Query query;
    for (const std::string& word : SplitIntoWords(text))
    {
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                query.minus_words.insert(query_word.data);
            }
            else
            {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const
{
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}