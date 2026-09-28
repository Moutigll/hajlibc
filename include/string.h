/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file string.h
 * @brief C standard string and memory functions.
 * @Created: 2026/09/24 22:10:46 by Moutig
 * @Updated: 2026/09/26 04:15:11 by Moutig
 *
 * This header declares the C standard string and memory
 * functions, plus a few POSIX/GNU extensions (memrchr, strdup,
 * strndup, strtok_r, strcasestr, bzero, bcopy, stpcpy, stpncpy).
 */

#ifndef _STRING_H
# define _STRING_H

# include <bits/compiler.h>
# include <stddef.h>

# if defined(__cplusplus)
extern "C" {
# endif

/* ----- Memory operations ----- */

/**
 * @brief Copy n bytes from src to dest. Regions must not overlap.
 *
 * @param dest Destination buffer.
 * @param src  Source buffer.
 * @param n    Number of bytes to copy.
 * @return dest.
 */
void	*memcpy(void *__HAJ_RESTRICT dest, const void *__HAJ_RESTRICT src, size_t n);

/**
 * @brief Copy n bytes from src to dest. Regions may overlap.
 *
 * @param dest Destination buffer.
 * @param src  Source buffer.
 * @param n    Number of bytes to copy.
 * @return dest.
 */
void	*memmove(void *dest, const void *src, size_t n);

/**
 * @brief Fill the first n bytes of the memory area pointed to by s with the constant byte c.
 *
 * @param s Pointer to the memory area to fill.
 * @param c Byte value to set (converted to unsigned char).
 * @param n Number of bytes to fill.
 * @return s.
 */
void	*memset(void *s, int c, size_t n);

#if defined(__HAJ_SOURCE)
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)	/* Fill memory with zero bytes. */
#endif

/**
 * @brief Compare the first n bytes of two memory areas.
 *
 * @param s1 Pointer to the first memory area.
 * @param s2 Pointer to the second memory area.
 * @param n  Number of bytes to compare.
 * @return An integer less than, equal to, or greater than zero if the first n bytes of s1 is found,
 *         respectively, to be less than, to match, or be greater than the first n bytes of s2.
 */
int		memcmp(const void *s1, const void *s2, size_t n);

/**
 * @brief Locate the first occurrence of c (converted to an unsigned char) in the first n bytes of s.
 *
 * Each byte is interpreted as an unsigned char.
 * The function returns a pointer to the matching byte or NULL if the character does not occur in the given memory area.
 * @param s Pointer to the memory area to search.
 * @param c Byte value to search for (converted to unsigned char).
 * @param n Number of bytes to search.
 * @return A pointer to the matching byte or NULL if the character does not occur in the given memory area.
 */
void	*memchr(const void *s, int c, size_t n);

#if defined(__HAJ_SOURCE)
/**
 * @brief Locate the last occurrence of c (converted to an unsigned char) in the first n bytes of s.
 *
 * Each byte is interpreted as an unsigned char.
 * The function returns a pointer to the matching byte or NULL if the character does not occur in the given memory area.
 * @param s Pointer to the memory area to search.
 * @param c Byte value to search for (converted to unsigned char).
 * @param n Number of bytes to search.
 * @return A pointer to the matching byte or NULL if the character does not occur in the given memory area.
 */
void	*memrchr(const void *s, int c, size_t n);
#endif

/**
 * @brief Copy bytes from src to dest until the character c (converted to an unsigned char) is found.
 *
 * The function copies bytes from src to dest until it encounters the character c (converted to an unsigned char) or reaches n bytes.
 * @param dest Destination buffer.
 * @param src  Source buffer.
 * @param c    Character to stop copying at (converted to an unsigned char).
 * @param n    Maximum number of bytes to copy.
 * @return A pointer to the byte after the first occurrence of c in the copied data, or NULL if c is not found within n bytes.
 */
void	*memccpy(void *__HAJ_RESTRICT dest, const void *__HAJ_RESTRICT src, int c, size_t n);



/* ----- String operations ----- */

/**
 * @brief Calculate the length of a null-terminated string.
 *
 * The function computes the length of the string s, excluding the terminating null byte ('\0').
 * @param s Pointer to the null-terminated string.
 * @return The number of characters in the string, excluding the null terminator.
 */
size_t	strlen(const char *s);

/**
 * @brief Calculate the length of a null-terminated string, up to a maximum of maxlen bytes.
 *
 * The function computes the length of the string s, excluding the terminating null byte ('\0'),
 * but at most maxlen bytes are examined.
 * @param s Pointer to the null-terminated string.
 * @param maxlen Maximum number of bytes to examine.
 * @return The number of characters in the string, excluding the null terminator, but at most maxlen.
 */
size_t	strnlen(const char *s, size_t maxlen);

/**
 * @brief Compare two null-terminated strings.
 *
 * The function compares the string s1 to the string s2.
 * It returns an integer less than, equal to, or greater than zero if s1 is found,
 * respectively, to be less than, to match, or be greater than s2.
 * @param s1 Pointer to the first null-terminated string.
 * @param s2 Pointer to the second null-terminated string.
 * @return An integer indicating the relationship between the two strings.
 */
int		strcmp(const char *s1, const char *s2);

/**
 * @brief Compare two null-terminated strings, up to a maximum of n characters.
 *
 * The function compares the string s1 to the string s2, but at most n characters are compared.
 * It returns an integer less than, equal to, or greater than zero if s1 is found,
 * respectively, to be less than, to match, or be greater than s2.
 * @param s1 Pointer to the first null-terminated string.
 * @param s2 Pointer to the second null-terminated string.
 * @param n Maximum number of characters to compare.
 * @return An integer indicating the relationship between the two strings.
 */
int		strncmp(const char *s1, const char *s2, size_t n);

/**
 * @brief Find the first occurrence of a character in a string.
 *
 * The function searches for the first occurrence of the character c (converted to an unsigned char) in the string s.
 * @param s Pointer to the null-terminated string.
 * @param c Character to search for (converted to an unsigned char).
 * @return A pointer to the first occurrence of c in s, or NULL if c is not found.
 */
char	*strchr(const char *s, int c);

/**
 * @brief Find the last occurrence of a character in a string.
 *
 * The function searches for the last occurrence of the character c (converted to an unsigned char) in the string s.
 * @param s Pointer to the null-terminated string.
 * @param c Character to search for (converted to an unsigned char).
 * @return A pointer to the last occurrence of c in s, or NULL if c is not found.
 */
char	*strrchr(const char *s, int c);

#if defined(__HAJ_SOURCE)
/**
 * @brief Find the first occurrence of a character in a string, or the end of the string.
 *
 * The function searches for the first occurrence of the character c (converted to an unsigned char) in the string s.
 * If the character is not found, it returns a pointer to the end of the string.
 * @param s Pointer to the null-terminated string.
 * @param c Character to search for (converted to an unsigned char).
 * @return A pointer to the first occurrence of c in s, or a pointer to the end of s if c is not found.
 */
char	*strchrnul(const char *s, int c);
#endif
char	*__strchrnul(const char *s, int c);

/**
 * @brief Copy a null-terminated string from src to dest.
 *
 * The function copies the string pointed to by src,
 * including the terminating null byte ('\0'), to the buffer pointed to by dest.
 * @param dest Destination buffer.
 * @param src  Source null-terminated string.
 * @return dest.
 */
char	*strcpy(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src);

/**
 * @brief Copy a null-terminated string from src to dest, up to a maximum of n characters.
 *
 * The function copies at most n characters from the string pointed to by src,
 * including the terminating null byte ('\0'), to the buffer pointed to by dest.
 * @param dest Destination buffer.
 * @param src  Source null-terminated string.
 * @param n    Maximum number of characters to copy (including the null terminator).
 * @return dest.
 */
char	*stpcpy(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src);

/**
 * @brief Copy a null-terminated string from src to dest, up to a maximum of n characters.
 *
 * The function copies at most n characters from the string pointed to by src,
 * including the terminating null byte ('\0'), to the buffer pointed to by dest.
 * @param dest Destination buffer.
 * @param src  Source null-terminated string.
 * @param n    Maximum number of characters to copy (including the null terminator).
 * @return dest.
 */
char	*strncpy(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src, size_t n);

/**
 * @brief Copy a null-terminated string from src to dest, up to a maximum of n characters.
 *
 * The function copies at most n characters from the string pointed to by src,
 * including the terminating null byte ('\0'), to the buffer pointed to by dest.
 * @param dest Destination buffer.
 * @param src  Source null-terminated string.
 * @param n    Maximum number of characters to copy (including the null terminator).
 * @return dest.
 */
char	*stpncpy(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src, size_t n);

#if defined(__HAJ_SOURCE)
/**
 * @brief Copy a null-terminated string from src to dest, up to a maximum of size - 1 characters.
 *
 * The function copies at most size - 1 characters from the string pointed to by src,
 * including the terminating null byte ('\0'), to the buffer pointed to by dest.
 * @param dest Destination buffer.
 * @param src  Source null-terminated string.
 * @param size Size of the destination buffer.
 * @return The total length of the string it tried to create (length of src).
 */
size_t	strlcpy(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src, size_t size);
#endif

/**
 * @brief Append a null-terminated string src to the end of dest.
 *
 * The function appends the string pointed to by src to the end of the string pointed to by dest.
 * The dest string must have enough space to hold the resulting concatenated string.
 * @param dest Destination buffer containing a null-terminated string.
 * @param src  Source null-terminated string to append.
 * @return dest.
 */
char	*strcat(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src);

/**
 * @brief Append at most n characters from the null-terminated string src to the end of dest.
 *
 * The function appends at most n characters from the string pointed to by src to the end of the string pointed to by dest.
 * The dest string must have enough space to hold the resulting concatenated string.
 * @param dest Destination buffer containing a null-terminated string.
 * @param src  Source null-terminated string to append.
 * @param n    Maximum number of characters to append from src.
 * @return dest.
 */
char	*strncat(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src, size_t n);

#if defined(__HAJ_SOURCE)
/**
 * @brief Append the null-terminated string src to the end of dest, ensuring that the total length does not exceed size - 1 characters.
 *
 * The function appends the string pointed to by src to the end of the string pointed to by dest,
 * ensuring that the total length of the resulting string does not exceed size - 1 characters,
 * and that the resulting string is null-terminated.
 * @param dest Destination buffer containing a null-terminated string.
 * @param src  Source null-terminated string to append.
 * @param size Size of the destination buffer.
 * @return The total length of the string it tried to create (initial length of dest plus length of src).
 */
size_t	strlcat(char *__HAJ_RESTRICT dest, const char *__HAJ_RESTRICT src, size_t size);
#endif

/**
 * @brief Calculate the length of the initial segment of s which contains only characters from accept.
 * @param s The string to be analyzed.
 * @param accept The string containing the characters to be accepted.
 * @return The length of the initial segment of s which contains only characters from accept.
 */
size_t	strspn(const char *s, const char *accept);

/**
 * @brief Calculate the length of the initial segment of s which contains no characters from reject.
 * @param s The string to be analyzed.
 * @param reject The string containing the characters to be rejected.
 * @return The length of the initial segment of s which contains no characters from reject.
 */
size_t	strcspn(const char *s, const char *reject);

/**
 * @brief Locate the first occurrence in the string s of any of the characters in the string accept.
 * @param s The string to be scanned.
 * @param accept The string containing the characters to match.
 * @return A pointer to the first occurrence in s of any of the characters in accept, or NULL if no such character is found.
 */
char	*strpbrk(const char *s, const char *accept);

/**
 * @brief Locate the first occurrence of the null-terminated string needle in the null-terminated string haystack.
 * @param haystack The string to be scanned.
 * @param needle The string to be searched for.
 * @return A pointer to the beginning of the located substring, or NULL if the substring is not found.
 */
char	*strstr(const char *haystack, const char *needle);

#if defined(__HAJ_SOURCE)
/**
 * @brief Locate the first occurrence of the null-terminated string needle in the null-terminated string haystack, ignoring case.
 * @param haystack The string to be scanned.
 * @param needle The string to be searched for.
 * @return A pointer to the beginning of the located substring, or NULL if the substring is not found.
 */
char	*strcasestr(const char *haystack, const char *needle);
#endif

/**
 * @brief Tokenize a string into substrings based on delimiters.
 *
 * The function splits the string str into tokens, which are sequences of characters separated by any of the characters in delim.
 * On the first call, str should point to the string to be tokenized. Subsequent calls should pass NULL for str to continue tokenizing the same string.
 * @param str Pointer to the string to be tokenized (or NULL for subsequent calls).
 * @param delim String containing delimiter characters.
 * @return A pointer to the next token, or NULL if there are no more tokens.
 */
char	*strtok(char *__HAJ_RESTRICT str, const char *__HAJ_RESTRICT delim);

/**
 * @brief Tokenize a string into substrings based on delimiters, in a reentrant manner.
 *
 * The function splits the string str into tokens, which are sequences of characters separated by any of the characters in delim.
 * On the first call, str should point to the string to be tokenized. Subsequent calls should pass NULL for str to continue tokenizing the same string.
 * The saveptr argument is used to maintain context between successive calls that parse the same string.
 * @param str Pointer to the string to be tokenized (or NULL for subsequent calls).
 * @param delim String containing delimiter characters.
 * @param saveptr Pointer to a char* variable that is used internally by strtok_r to maintain context between successive calls.
 * @return A pointer to the next token, or NULL if there are no more tokens.
 */
char	*strtok_r(char *__HAJ_RESTRICT str, const char *__HAJ_RESTRICT delim, char **__HAJ_RESTRICT saveptr);

#if defined(__HAJ_SOURCE)
/**
 * @brief Split a string into tokens, using the specified delimiter.
 *
 * This function splits the string pointed to by stringp into tokens, which are sequences of characters separated by any of the characters in delim.
 * The function modifies the original string by replacing the delimiter characters with null terminators.
 * @param stringp Pointer to a pointer to the string to be split (or NULL for subsequent calls).
 * @param delim String containing delimiter characters.
 * @return A pointer to the next token, or NULL if there are no more tokens.
 */
char	*strsep(char **__HAJ_RESTRICT stringp, const char *__HAJ_RESTRICT delim);
#endif

# if defined(__cplusplus)
}
# endif

#endif /* _STRING_H */
