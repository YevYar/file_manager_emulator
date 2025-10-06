#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <string_view>

/**
 * \brief Checks whether the given string represents a file basename.
 * 
 * This is a "rough" guess about the file
 * type based on the presence of '.', since dirs can also have '.' in their names.
 */
bool isFilename(std::string_view filename);

/**
 * \brief Checks whether the provided character is a space.
 * 
 * \return Non-zero value if the character is a whitespace character, zero otherwise.
 */
int isSpace(unsigned char c);

/**
 * \brief Helper: trims leading and trailing whitespaces from a string.
 */
void trim(std::string& str);

#endif  // HELPERS_H
