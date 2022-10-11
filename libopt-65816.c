/**
 *  opt-65816 - Assembly code Optimizer for WDC65816
 *
 * Assembly code optimizer for the WDC65816 processor produced
 * by the 65816 Tiny C Compiler (816-tcc) developed by AlekMaul.
 *
 * This library is a C port of the 816-opt python
 * tool developed by nArnoSNES.
 *
 *  Copyright (c) 2022 Kobenairb
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "opt-65816.h"

/*!
 * @brief Enable verbosity if OPT_816_QUIET is set
 *     OPT_816_QUIET=0 or unset (no verbosity)
 *     OPT_816_QUIET=1          (normal verbosity)
 *     OPT_816_QUIET=2          (for debug purpose)
 * @return
 */
int verbosity()
{
    char *OPT_816_QUIET = getenv("OPT_816_QUIET");

    if (!OPT_816_QUIET || *OPT_816_QUIET == '0')
        return 0;
    else if (OPT_816_QUIET && *OPT_816_QUIET == '1')
        return 1;
    else if (OPT_816_QUIET && *OPT_816_QUIET == '2')
        return 2;
    /* Unmaned case, should we exit? */
    return 3;
}

/*!
 * @brief Free pointers and pointers to pointers
 * @param l
 * @param u
 */
void FreeDynArray(char **l, int u)
{
    for (size_t i = 0; i < u; i++)
    {
        free(l[i]);
    }
    free(l);
}

/*!
 * @brief Check if a string starts with b string
 * @param a
 * @param b
 * @return
 */
int StartsWith(const char *a, const char *b)
{
    if (strncmp(a, b, strlen(b)) == 0)
        return 1;

    return 0;
}

/*!
 * @brief Check if a string ends with b string
 * @param a
 * @param b
 * @return
 */
int EndsWith(const char *a, const char *b)
{
    size_t alen = strlen(a);
    size_t blen = strlen(b);

    if (strcmp(a + alen - blen, b) == 0)
        return 1;
    return 0;
}

/* Check if b string is in a string */

/*!
 * @brief Check if b string is in a string
 * @param a
 * @param b
 * @return
 */
int IsInText(const char *a, const char *b)
{
    if (strstr(a, b) != NULL)
        return 1;
    return 0;
}

/*!
 * @brief get the smalest number between two int
 * @param a
 * @param b
 * @return
 */
int FindMin(const int a, const int b)
{
    if (a > b)
        return b;
    else
        return a;
}

/*!
 * @brief Remove leading/trailing whitespaces
 * @param str
 * @return
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

/*!
 * @brief checks if it touches the accumulator register
 * @param a
 * @return
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

/*!
 * @brief Checks if the line alters the control flow
 * @param a
 * @return
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
 * @brief Structure to store RegMatchGroups
 */
typedef struct RegDynArray
{
    size_t status;
    regex_t regexCompiled;
    char **groups;
} RegDynArray;

/**
 * @brief
 * @param source
 * @param regex
 * @param maxgroup
 * @return
 */
RegDynArray RegMatchGroups(char *source, char *regex, const unsigned int maxgroup)
{
    regex_t regexCompiled;
    regmatch_t groupArray[maxgroup];
    char *cursor = source;
    size_t used = 0;
    char **groups = NULL;

    if (regcomp(&regexCompiled, regex, REG_EXTENDED))
    {
        printf("Could not compile regular expression.\n");
        exit(EXIT_FAILURE);
    };

    if (regexec(&regexCompiled, cursor, maxgroup, groupArray, 0))
    {
        RegDynArray r = {0, regexCompiled, groups};
        return r;
    }

    size_t len;
    size_t g;

    size_t nptrs = NPTRS;

    if ((groups = malloc(nptrs * sizeof *groups)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    for (g = 0; g < maxgroup; g++)
    {
        if (groupArray[g].rm_so == (size_t)-1)
        {
            break; // No more groups
        }

        char cursorCopy[strlen(cursor) + 1];
        strcpy(cursorCopy, cursor);
        cursorCopy[groupArray[g].rm_eo] = 0;

        len = strlen(cursorCopy + groupArray[g].rm_so);

        /* check if realloc of lines needed */
        if (used == nptrs)
        {
            printf("%s\n", source);
            /* always realloc using temporary pointer (doubling no. of pointers) */
            void *tmp = realloc(groups, (2 * nptrs) * sizeof *groups);
            if (!tmp)
            {
                perror("realloc-lines");
                break;
            }
            /* assign reallocated block to lines */
            groups = tmp;
            /* update no. of pointers allocatd */
            nptrs *= 2;
        }

        /* allocate/validate storage for line */
        if (!(groups[used] = malloc(len + 1)))
        {
            perror("malloc-lines[used]");
            break;
        }

        memcpy(groups[used], cursorCopy + groupArray[g].rm_so, len + 1);
        ++used;
    }

    RegDynArray r = {1, regexCompiled, groups};

    return r;
}
