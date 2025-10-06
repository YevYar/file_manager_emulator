#include "helpers.h"

#include <algorithm>

namespace
{
constexpr inline auto fileDelimiter = '.';

}  // namespace

bool isFilename(const std::string_view filename)
{
    return filename.find_first_of(fileDelimiter) != std::string_view::npos;
}

int isSpace(const unsigned char c)
{
    return std::isspace(c);
}

void trim(std::string& str)
{
    const auto first = std::find_if_not(str.begin(), str.end(), isSpace);
    const auto last  = std::find_if_not(str.rbegin(), str.rend(), isSpace).base();

    if (first < last)
    {
        str.assign(first, last);
    }
    else
    {
        str.clear();  // All spaces
    }
}
