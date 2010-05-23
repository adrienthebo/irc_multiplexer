/* utilities.c
 *
 * Implementation for utility functions
 */

#include <stdlib.h>
#include <string.h>
#include "utilities.h"

/* 
 * Allocates memory for the concatenation of two strings, frees the old
 * string, and replaces the old string with the new string.
 *
 * TODO revisit this and think about how badly this would explode if a
 * static string was inputted.
 */
void strn_append(char **old_str, char *append_str, size_t n) {

    if(*old_str == NULL) {
	*old_str = malloc(n + 1);
	memset(*old_str, 0, n + 1);
	strcpy(*old_str, append_str);
    }
    else {
	int new_str_len = strlen(*old_str) + n;
	char * new_str = malloc(new_str_len + 1);
	memset(new_str, 0, new_str_len + 1);

	strcat(new_str, *old_str);
	strncat(new_str, append_str, n);

	free(*old_str);
	*old_str = new_str;
    }
}

/* 
 * Calls strn_append to append the entire append_str.
 */
void str_append(char **old_str, char *append_str) {
    strn_append(old_str, append_str, strlen(append_str));
}

