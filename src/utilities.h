/* utilities.h
 *
 * Common string manipulation utilities
 */

#ifndef _UTILITIES_H
#define _UTILITIES_H

/* 
 * Allocates memory for the concatenation of two strings, 
 * replaces the old string with the new string, and returns the old strung
 */
char * strn_append(char **old_str, char *append_str, size_t n);

/* 
 * Calls strn_append to append the entire append_str.
 */
char * str_append(char **old_str, char *append_str);

#endif /* _UTILITIES_H */
