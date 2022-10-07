/*
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

#include "header.h"

/* Enable verbosity if OPT_816_QUIET is set
    OPT_816_QUIET=0 or unset (no verbosity)
    OPT_816_QUIET=1          (normal verbosity)
    OPT_816_QUIET=2          (for debug purpose) */
unsigned int verbosity()
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

/* char *concat(const char *s1, const char *s2)
{
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    char *result = malloc(len1 + len2 + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2 + 1); // +1 to copy the null-terminator
    return result;
} */

/* Check if a string starts with */
int StartsWith(const char *a, const char *b)
{
    if (strncmp(a, b, strlen(b)) == 0)
        return 1;

    return 0;
}

/* Check if a string starts with */
int EndsWith(const char *a, const char *b)
{
    size_t alen = strlen(a);
    size_t blen = strlen(b);

    if (strcmp(a + alen - blen, b) == 0)
        return 1;
    return 0;
}

/* get the smalest number between two int */
int FindMin(const int a, const int b)
{
    if (a > b)
        return b;
    else
        return a;
}

/* Check return code */
void CheckRc(const char *message, const unsigned int rc)
{
    if (rc)
    {
        perror(message);
        exit(rc);
    }

    perror(message);
}

/* Remove leading/trailing whitespaces */
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

int ChangesAccu(const char *a)
{
    if (a)
        if ((a[2] == 'a' && (!StartsWith(a, "pha") || !StartsWith(a, "sta"))) ||
            (strlen(a) == 5 && EndsWith(a, " a")))
            return 1;

    return 0;
}
/* Checks if the line alters the control flow */
int IsControl(const char *a)
{
    if (a)
        if (EndsWith(a, ":") ||
            (StartsWith(a, "j") ||
             StartsWith(a, "b") ||
             StartsWith(a, "l") ||
             StartsWith(a, "-") ||
             StartsWith(a, "+")))
            return 1;

    return 0;
}