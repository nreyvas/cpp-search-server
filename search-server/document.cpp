#include "document.h"

using namespace std::string_literals;

std::ostream& operator<<(std::ostream& os, const Document& d)
{
    return os << "{ document_id = "s << d.id
        << ", relevance = "s << d.relevance
        << ", rating = "s << d.rating << " }";
}