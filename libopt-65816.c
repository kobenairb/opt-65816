/*
 * opt-65816 - Assembly code optimizer for WDC65816.
 *
 * Description: Assembly code optimizer for the WDC65816 processor produced
 * by the 65816 Tiny C Compiler (816-tcc).
 * This library is a C port of the 816-opt python tool.
 *
 * Author: kobenairb (kobenairb@gmail.com).
 *
 * Copyright (c) 2022.
 *
 * This project is released under the GNU Public License.
 *
 */

#include "opt-65816.h"

/**
 * @struct DynArray
 * @brief Structure to store the return of BssStore and TidyFile.
 * @var DynArray::text
 * Member 'text' contains the array of strings.
 * @var DynArray::used
 * Member 'used' contains the number of strings (elements)
 * in the array.
 */
typedef struct DynArray
{
    char **text;
    size_t used;
} DynArray;

/**
 * @struct RegDynArray
 * @brief Structure to store the return of RegMatchGroups.
 * @var RegDynArray::status
 * Member 'status' contains the status if the function.
 * 1 if the regexp matchs groups, or 0 if not.
 * @var RegDynArray::regexCompiled
 * Member 'regexCompiled' contains the adress of
 the compiled regex.
 * @var RegDynArray::groups
 * Member 'groups' contains the array of strings
 * of matched groups.
 */
typedef struct RegDynArray
{
    size_t status;
    char **groups;
    size_t used;
} RegDynArray;

/**
 * @brief Enable verbosity if OPT_65816_VERBOSE is set
 * OPT_65816_VERBOSE can be set with the following value:
 * 0, 1 or 2, or just unset.
 * @return 0 (disable), 1 (normal) or 2 (debug)
 */
int verbosity()
{
    char *OPT_65816_VERBOSE = getenv("OPT_65816_VERBOSE");

    if (!OPT_65816_VERBOSE || *OPT_65816_VERBOSE == '0')
        return 0;
    else if (*OPT_65816_VERBOSE == '1')
        return 1;
    else if (*OPT_65816_VERBOSE == '2')
        return 2;
    /* Unmaned case, should we exit? */
    return 3;
}

/**
 * @brief Free pointers and double pointers.
 * @param p Pointer.
 * @param n Number of pointer to pointer
 */
void FreeDynArray(char **p, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        free(p[i]);
    }
    free(p);
}

/**
 * @brief Check if the source string starts with the pattern string.
 * @param source Source string.
 * @param pattern Pattern string.
 * @return 1 (true) or 0 (false).
 */
int StartsWith(const char *source, const char *pattern)
{
    if (strncmp(source, pattern, strlen(pattern)) == 0)
        return 1;

    return 0;
}

/**
 * @brief Check if the source string ends with the pattern string.
 * @param source Source string.
 * @param pattern Pattern string.
 * @return 1 (true) or 0 (false).
 */
int EndsWith(const char *source, const char *pattern)
{
    size_t slen = strlen(source);
    size_t plen = strlen(pattern);

    if (strcmp(source + slen - plen, pattern) == 0)
        return 1;
    return 0;
}

/**
 * @brief Check if the pattern string is in the source string.
 * @param source Source string.
 * @param pattern Pattern string.
 * @return 1 (true) or 0 (false)
 */
int IsInText(const char *source, const char *pattern)
{
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
int FindMin(const int a, const int b)
{
    if (a > b)
        return b;
    else
        return a;
}

/**
 * @brief Remove leading/trailing whitespaces.
 * @param str the string to trim.
 * @return The trimmed string.
 */
char *TrimWhiteSpace(char *str)
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
 * @brief Checksif it touches the accumulator register.
 * @param a The asm instruction.
 * @return 1 (true) or 0 (false).
 */
int ChangesAccu(const char *a)
{
    if (strlen(a) > 2)
    {
        if (a[2] == 'a' && (!StartsWith(a, "pha") && !StartsWith(a, "sta")))
            return 1;
        if (strlen(a) == 5 && EndsWith(a, " a"))
            return 1;
    }

    return 0;
}

/**
 * @brief Check if the line alters the control flow.
 * @param a The asm instruction.
 * @return 1 (true) or 0 (false).
 */
int IsControl(const char *a)
{
    if (strlen(a) > 0)
    {
        if (EndsWith(a, ":"))
        {
            return 1;
        }
        if (StartsWith(a, "j") ||
            StartsWith(a, "b") ||
            StartsWith(a, "-") ||
            StartsWith(a, "+"))
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Wrapper to match groups with regex.
 * @param source The string.
 * @param regex The POSIX regex.
 * @param maxGroups The maximum number of groups to match.
 * @return A structure (RegDynArray).
 */
RegDynArray RegMatchGroups(char *source, char *regex, const size_t maxGroups)
{
    regex_t regexCompiled;
    regmatch_t groupArray[maxGroups];
    size_t used = 0;
    char *cursor = source;
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

        RegDynArray r = {1, groups, used};
        return r;
    }
    else if (re == REG_NOMATCH)
    {
        regfree(&regexCompiled);

        RegDynArray r = {0, groups, used};
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
