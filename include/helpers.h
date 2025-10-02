#ifndef HELPERS_H
#define HELPERS_H

#include <string>

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
