#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(std::string_view text)
{
    std::vector<std::string_view> result;

    size_t end_pos = text.npos;
    while (true)
    {
        while (!text.empty() && text.front() == ' ')
        {
            text.remove_prefix(1);
        }
        if (text.empty())
        {
            break;
        }
        size_t space_pos = text.find(' ');
        if (space_pos != end_pos)
        {
            result.push_back(text.substr(0, space_pos));
            text.remove_prefix(space_pos);
        }
        else
        {
            result.push_back(text.substr());
            break;
        }
    }
    return result;
}