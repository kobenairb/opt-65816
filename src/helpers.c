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
 * @param s dynArray structure.
 */
void freedynArray(dynArray s)
{
    for (size_t i = 0; i < s.used; i++)
    {
        free(s.arr[i]);
    }
    free(s.arr);
}

/**
 * @brief Check if two strings are identical.
 * @param str1 Source string.
 * @param str2 Pattern string.
 * @return 1 (true) or 0 (false).
 */
int matchStr(const char *str1, const char *str2)
{
    if (!str1 || !str2)
        return 0; // handle NULL inputs

    return strcmp(str1, str2) == 0;
}

/**
 * @brief Check if the source string starts with the prefix string.
 * @param source Source string.
 * @param prefix Pattern string.
 * @return 1 (true) or 0 (false).
 */
int startWith(const char *source, const char *prefix)
{
    if (!source || !prefix)
        return 0; // handle NULL inputs

    size_t prefix_len = strlen(prefix);
    return strncmp(source, prefix, prefix_len) == 0;
}

/**
 * @brief Check if the source string ends with the prefix string.
 * @param source Source string.
 * @param prefix Pattern string.
 * @return 1 (true) or 0 (false).
 */
int endWith(const char *source, const char *suffix)
{
    if (!source || !suffix)
        return 0; // handle NULL inputs

    size_t source_len = strlen(source);
    size_t suffix_len = strlen(suffix);
    if (source_len < suffix_len)
        return 0; // suffix is longer than source

    return strcmp(source + source_len - suffix_len, suffix) == 0;
}

/**
 * @brief Check if the pattern string is in the source string.
 * @param source Source string.
 * @param pattern Pattern string.
 * @return 1 (true) or 0 (false)
 */
int isInText(const char *source, const char *pattern)
{
    if (!source || !pattern)
        return 0; // handle NULL inputs

    return strstr(source, pattern) != NULL;
}

/**
 * @brief Compare two numbers.
 * @param a First number.
 * @param b Second number.
 * @return The smallest number.
 */
int min(const int a, const int b)
{
    return a < b ? a : b;
}

/**
 * @brief Compare two numbers.
 * @param a First number.
 * @param b Second number.
 * @return The bigger number.
 */
int max(const int a, const int b)
{
    return a > b ? a : b;
}

/**
 * @brief Remove leading/trailing whitespaces.
 * @param str the string to trim.
 * @return The trimmed string.
 */
char *trimWhiteSpace(char *str)
{
    size_t len = strlen(str);

    /* Trim leading space */
    while (isspace((unsigned char)*str))
        str++, len--;

    /* Only spaces */
    if (len == 0)
        return str;

    /* Trim trailing space */
    while (isspace((unsigned char)str[len - 1]))
        len--;

    /* Write new null terminator character */
    str[len] = '\0';

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
        Inspired by Padymko:
        https://stackoverflow.com/a/42283266
    */

    int str_len = strlen(str);

    // if a string is empty, returns nothing
    if (str_len == 0)
        return NULL;

    // for negative indexes "slice_from" must be less "slice_to"
    if (slice_to < 0 && slice_from < slice_to)
    {
        // if "slice_to" goes beyond permissible limits
        if (abs(slice_to) > str_len - 1)
            return NULL;

        // if "slice_from" goes beyond permissible limits
        slice_from = max(-str_len, slice_from);

        // calculate the buffer length
        int buffer_len = min(slice_to - slice_from, str_len + slice_from);

        char *buffer = malloc(buffer_len + 1);
        memcpy(buffer, &str[str_len + slice_from], buffer_len);
        buffer[buffer_len] = '\0';

        return buffer;
    }

    // for positive indexes "slice_from" must be more "slice_to"
    if (slice_from >= 0 && slice_to > slice_from)
    {
        // if "slice_from" goes beyond permissible limits
        if (slice_from > str_len - 1)
            return NULL;

        // calculate the buffer length
        int buffer_len = min(slice_to - slice_from, str_len - slice_from);

        char *buffer = malloc(buffer_len + 1);
        memcpy(buffer, &str[slice_from], buffer_len);
        buffer[buffer_len] = '\0';

        return buffer;
    }

    // otherwise, returns NULL
    return NULL;
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
    size_t orig_len = strlen(orig);
    size_t rep_len = strlen(rep);

    if (!(p = strstr(str, orig)))
        return str;

    memcpy(buffer, str, p - str);
    buffer[p - str] = '\0';

    strcpy(buffer + (p - str), rep);
    strcat(buffer + (p - str + rep_len), p + orig_len);

    return buffer;
}

/**
 * @brief Split a string.
 * @param str The string.
 * @param sep The separator (one haracter only).
 * @param pos The position of the substring.
 * @return The substring or NULL (if the
 * the separator is more than one character or if the substring
 * doesn't exist for the given position.
 */
char *splitStr(char *str, char *sep, size_t pos)
{
    if (strlen(sep) == 1)
    {
        char tmp[sizeof(str)];

        strcpy(tmp, str);
        char *token;
        char *saveptr;
        // token       = strtok(tmp, sep);
        token = strtok_r(tmp, sep, &saveptr);

        // Loop through the string to extract all
        // other tokens until the given position.
        for (size_t i = 0; i < pos; i++)
        {
            token = strtok_r(NULL, sep, &saveptr);
        }

        return token;
    }

    return NULL;
}

/**
 * @brief Wrapper to match groups with regex.
 * @param string The string.
 * @param regex The POSIX regex.
 * @param maxGroups The maximum number of groups to match.
 * @return A structure (dynArray).
 */
#include <pcre2.h>

dynArray pcre2MatchGroups(char *string, char *regex, const size_t maxGroups)
{
    /*
        Inspired by Ianmackinnon:
        https://gist.github.com/ianmackinnon/3294587
    */

    int errorcode;
    PCRE2_SIZE erroroffset;
    pcre2_code *regexCompiled;
    pcre2_match_data *matchData;
    PCRE2_SIZE *ovector;
    int rc;

    dynArray regexgroup;
    regexgroup.used = 0;

    regexCompiled = pcre2_compile(
        (PCRE2_SPTR)regex,        /* the pattern */
        PCRE2_ZERO_TERMINATED,    /* indicates pattern is zero-terminated */
        0,                        /* default options */
        &errorcode,               /* for error code */
        &erroroffset,             /* for error offset */
        NULL                      /* use default compile context */
    );

    if (regexCompiled == NULL)
    {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
        fprintf(stderr, "Could not compile regular expression: %s\n", buffer);
        exit(EXIT_FAILURE);
    }

    matchData = pcre2_match_data_create(maxGroups, NULL);

    rc = pcre2_match(
        regexCompiled,             /* the compiled pattern */
        (PCRE2_SPTR)string,        /* the subject string */
        strlen(string),            /* the length of the subject string */
        0,                         /* start offset in the subject */
        0,                         /* default options */
        matchData,                 /* block for storing the result */
        NULL                       /* use default match context */
    );

    if (rc < 0)
    {
        if (rc == PCRE2_ERROR_NOMATCH)
        {
            pcre2_code_free(regexCompiled);
            pcre2_match_data_free(matchData);

            regexgroup.arr = NULL;
            return regexgroup;
        }

        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(rc, buffer, sizeof(buffer));
        fprintf(stderr, "Regex match failed: %s\n", buffer);
        exit(EXIT_FAILURE);
    }

    ovector = pcre2_get_ovector_pointer(matchData);
    size_t g;
    for (g = 0; g < (size_t)rc; g++)
    {
        PCRE2_SIZE len = ovector[2 * g + 1] - ovector[2 * g];

        /* allocate storage for line */
        regexgroup.arr[regexgroup.used] = malloc(len + 1);
        memcpy(regexgroup.arr[regexgroup.used], string + ovector[2 * g], len);
        regexgroup.arr[regexgroup.used][len] = '\0';
        regexgroup.used += 1;
    }

    pcre2_match_data_free(matchData);
    pcre2_code_free(regexCompiled);

    return regexgroup;
}

/**
 * @brief Add string to array and update the length of the array.
 * @param text_opt The dynArray structure.
 * @param str The string to add.
 * @return the dynArray structure updated.
 */
dynArray pushToArray(dynArray text_opt, char *str)
{
    size_t len = strlen(str);
    dynArray updatedDynArray;

    if ((text_opt.arr[text_opt.used] = malloc(len + 1)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    strcpy(text_opt.arr[text_opt.used], str);
    text_opt.used++;

    updatedDynArray.arr = text_opt.arr;
    updatedDynArray.used = text_opt.used;

    return updatedDynArray;
}
