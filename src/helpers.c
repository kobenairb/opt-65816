/*
 * opt-65816 - Assembly code optimizer for the WDC 65816 processor.
 *
 * Description: Assembly code optimizer produced
 * by the 816 Tiny C Compiler (816-tcc).
 * This library is a C port of the 816-opt python tool.
 *
 * Author: kobenairb (kobenairb@gmail.com).
 *
 * Copyright (c) 2022.
 *
 * This project is released under the GNU Public License.
 *
 */

#include "helpers.h"

/**
 * @brief Free pointers.
 * @param p Pointers of pointer.
 * @param n Number of pointers of pointer.
 */
void freedynArray(char **p, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        free(p[i]);
    }
    free(p);
}

/**
 * @brief Check if two strings are identical.
 * @param str1 Source string.
 * @param str2 Pattern string.
 * @return 1 (true) or 0 (false).
 */
int matchString(const char *str1, const char *str2)
{
    if (strcmp(str1, str2) == 0)
        return 1;

    return 0;
}

/**
 * @brief Check if the source string starts with the prefix string.
 * @param source Source string.
 * @param prefix Pattern string.
 * @return 1 (true) or 0 (false).
 */
int startWith(const char *source, const char *prefix)
{
    // if a string is empty, returns false
    if (source[0] == '\0')
        return 0;

    if (strncmp(source, prefix, strlen(prefix)) == 0)
        return 1;

    return 0;
}

/**
 * @brief Check if the source string ends with the prefix string.
 * @param source Source string.
 * @param prefix Pattern string.
 * @return 1 (true) or 0 (false).
 */
int endWith(const char *source, const char *prefix)
{
    // if a string is empty, returns false
    if (source[0] == '\0')
        return 0;

    size_t slen = strlen(source);
    size_t plen = strlen(prefix);

    if (slen < plen)
        return 0;

    if (strncmp(source + slen - plen, prefix, plen) == 0)
        return 1;
    return 0;
}

/**
 * @brief Check if the pattern string is in the source string.
 * @param source Source string.
 * @param pattern Pattern string.
 * @return 1 (true) or 0 (false)
 */
int isInText(const char *source, const char *pattern)
{
    // if a string is empty, returns false
    if (source[0] == '\0')
        return 0;

    if (strstr(source, pattern) != NULL)
        return 1;
    return 0;
}

/**
 * @brief Compare two numbers.
 * @param a First number.
 * @param b Second numbers.
 * @return The smallest number.
 */
int findMin(const int a, const int b)
{
    if (a && b && (a > b))
        return b;
    return a;
}

/**
 * @brief Remove leading/trailing whitespaces.
 * @param str the string to trim.
 * @return The trimmed string.
 */
char *trimWhiteSpace(char *str)
{
    char *end;

    /* Trim leading space */
    while (isspace((unsigned char)*str))
        str++;

    /* Only spaces */
    if (*str == 0)
        return str;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    /* Write new null terminator character */
    end[1] = '\0';

    return str;
}

/**
 * @brief Extracts a selection of string.
 * It supports both negative and positive indexes.
 * @param str The string.
 * @param slice_from Starting position (index).
 * @param slice_to Ending position (index).
 * @return A new string or NULL.
 */
char *sliceStr(char *str, int slice_from, int slice_to)
{
    /*
        From Padymko: https://stackoverflow.com/a/42283266
    */

    // if a string is empty, returns nothing
    if (str[0] == '\0')
        return NULL;

    char *buffer;
    int str_len, buffer_len;

    // for negative indexes "slice_from" must be less "slice_to"
    if (slice_to < 0 && slice_from < slice_to)
    {
        str_len = strlen(str);

        // if "slice_to" goes beyond permissible limits
        if (abs(slice_to) > str_len - 1)
            return NULL;

        // if "slice_from" goes beyond permissible limits
        if (abs(slice_from) > str_len)
            slice_from = (-1) * str_len;

        buffer_len = slice_to - slice_from;
        str += (str_len + slice_from);

        // for positive indexes "slice_from" must be more "slice_to"
    }
    else if (slice_from >= 0 && slice_to > slice_from)
    {
        str_len = strlen(str);

        // if "slice_from" goes beyond permissible limits
        if (slice_from > str_len - 1)
            return NULL;

        buffer_len = slice_to - slice_from;
        str += slice_from;

        // otherwise, returns NULL
    }
    else
        return NULL;

    buffer = calloc(buffer_len, sizeof(char) + 1);
    strncpy(buffer, str, buffer_len);
    return buffer;
}

/**
 * @brief Replace a substring in string by another substring.
 * @param str The string.
 * @param orig The substring to replace.
 * @param rep The substring to replace with.
 * @return The modified string.
 */
char *replaceStr(char *str, char *orig, char *rep)
{
    static char buffer[MAXLEN_LINE];
    char *p;

    if (!(p = strstr(str, orig)))
        return str;

    strncpy(buffer, str, p - str);
    buffer[p - str] = '\0';

    sprintf(buffer + (p - str), "%s%s", rep, p + strlen(orig));

    return buffer;
}

/**
 * @brief Wrapper to match groups with regex.
 * @param source The string.
 * @param regex The POSIX regex.
 * @param maxGroups The maximum number of groups to match.
 * @return A structure (regexdynArray).
 */
regexdynArray regexMatchGroups(char *source, char *regex, const size_t maxGroups)
{
    /*
        From Ianmackinnon https://gist.github.com/ianmackinnon/3294587
    */

    regex_t regexCompiled;
    regmatch_t groupArray[maxGroups];
    size_t used   = 0;
    char *cursor  = source;
    char **groups = NULL;
    size_t len;

    int re = regcomp(&regexCompiled, regex, REG_EXTENDED);

    if (re)
    {
        printf("Could not compile regular expression.\n");
        exit(EXIT_FAILURE);
    };

    re = regexec(&regexCompiled, cursor, maxGroups, groupArray, 0);

    if (!re)
    {
        size_t g;
        size_t nptrs = maxGroups;

        if ((groups = malloc(nptrs * sizeof *groups)) == NULL)
        {
            perror("malloc-lines");
            exit(EXIT_FAILURE);
        }

        for (g = 0; g < maxGroups; g++)
        {
            if ((size_t)groupArray[g].rm_so == (size_t)-1)
            {
                break; // No more groups
            }

            char cursorCopy[strlen(cursor) + 1];
            strcpy(cursorCopy, cursor);
            cursorCopy[groupArray[g].rm_eo] = 0;

            len = strlen(cursorCopy + groupArray[g].rm_so);

            /* allocate storage for line */
            groups[used] = malloc(len + 1);
            memcpy(groups[used], cursorCopy + groupArray[g].rm_so, len + 1);
            used += 1;
        }

        regfree(&regexCompiled);

        regexdynArray r = { 1, groups, used };
        return r;
    }
    else if (re == REG_NOMATCH)
    {
        regfree(&regexCompiled);

        regexdynArray r = { 0, groups, used };
        return r;
    }
    else
    {
        char msgbuf[100];
        regerror(re, &regexCompiled, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Regex match failed: %s\n", msgbuf);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Add string to array.
 * @param arr The array of strings.
 * @param str The string to add.
 * @param used The current last index of the index.
 * @return A structure (dynArray).
 */
dynArray pushToArray(char **arr, char *str, int used)
{
    size_t len = strlen(str);

    if ((arr[used] = malloc(len + 1)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    memcpy(arr[used], str, len + 1);
    used += 1;

    dynArray b = { arr, used };
    return b;
}
