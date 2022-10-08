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

/* TODO : not tested with Windows */
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#endif

#include "header.h"
#include "lib.c"

/* Structure to store an array
    and the number of elements */
typedef struct DynArray
{
    char **arr;
    size_t used;
} DynArray;

/* Create a dynamic string array from file
    without comment and leading/trailing white spaces */
DynArray TidyFile(const int argc, char **argv)
{
    /* fixed buffer to read each line */
    char buf[MAXLEN_LINE];
    /* pointer to pointer to hold collection of lines */
    char **lines = NULL;
    /* number of pointers available */
    size_t nptrs = NPTRS;
    /* number of pointers used */
    size_t used = 0;

    /* use filename provided as 1st argument (stdin by default) */
    FILE *fp = argc > 1 ? fopen(argv[1], "r") : stdin;

    /* validate file open for reading */
    if (!fp)
        CheckRc("file open failed", EXIT_FAILURE);

    /* allocate/validate block holding initial nptrs pointers */
    if ((lines = malloc(nptrs * sizeof *lines)) == NULL)
        CheckRc("malloc-lines", EXIT_FAILURE);

    /* read each line into buf */
    while (fgets(buf, MAXLEN_LINE, fp))
    {
        size_t len;
        /* trim \n, save length */
        buf[(len = strcspn(buf, "\n"))] = 0;

        if (!StartsWith(buf, COMMENT))
        {
            /* check if realloc of lines needed */
            if (used == nptrs)
            {
                /* always realloc using temporary pointer (doubling no. of pointers) */
                void *tmp = realloc(lines, (2 * nptrs) * sizeof *lines);
                if (!tmp)
                {
                    CheckRc("realloc-lines", 0);
                    break;
                }
                /* assign reallocated block to lines */
                lines = tmp;
                /* update no. of pointers allocatd */
                nptrs *= 2;
            }

            /* allocate/validate storage for line */
            if (!(lines[used] = malloc(len + 1)))
            {
                CheckRc("malloc-lines[used]", 0);
                break;
            }
            /* copy line from buf to lines[used] */
            memcpy(lines[used], TrimWhiteSpace(buf), len + 1);
            /* increment used pointer count */
            used += 1;
        }
    }
    /* close file if not stdin */
    if (fp != stdin)
        fclose(fp);

    DynArray r = {lines, used};
    return r;
}

/* Create a dynamic string array to store
    block bss instructions (first word) */
DynArray StoreBss(char **l, const int u)
{
    char **bss = NULL;
    char *saveptr;

    size_t used = 0;
    size_t bss_on = 0;
    size_t nptrs = NPTRS;

    if ((bss = malloc(nptrs * sizeof *bss)) == NULL)
        CheckRc("malloc-lines", EXIT_FAILURE);

    for (size_t i = 0; i < u; i++)
    {
        size_t len;
        len = strlen(l[i]);

        if (StartsWith(l[i], BSS_START))
        {
            bss_on = 1;
            continue;
        }
        if (StartsWith(l[i], BSS_END) && bss_on)
        {
            bss_on = 0;
            continue;
        }
        if (!StartsWith(l[i], BSS_START) && bss_on)
        {
            /* check if realloc of lines needed */
            if (used == nptrs)
            {
                /* always realloc using temporary pointer (doubling no. of pointers) */
                void *tmp = realloc(bss, (2 * nptrs) * sizeof *bss);
                if (!tmp)
                {
                    CheckRc("realloc-lines", 0);
                    break;
                }
                /* assign reallocated block to lines */
                bss = tmp;
                /* update no. of pointers allocatd */
                nptrs *= 2;
            }

            /* allocate/validate storage for line */
            if (!(bss[used] = malloc(len + 1)))
            {
                CheckRc("malloc-lines[used]", 0);
                break;
            }

            /* Store the first word of bss instruction */
            memcpy(bss[used], strtok_r(l[i], " ", &saveptr), len + 1);
            used += 1;
        }
    }

    DynArray b = {bss, used};
    return b;
}

/* Optimize ASM code */
void OptimizeAsm(char **l, const unsigned int u)
{
    /* total number of optimizations performed */
    // int totalopt = 0;
    /* have we OptimizeAsmd in this pass */
    // int opted = -1;
    /* optimization pass counter */
    // int opass = 0;

    size_t maxGroups = 3;
    regmatch_t groupArray[maxGroups];

    regex_t regexa,
        regexb,
        regexc,
        regexd;

    if (regcomp(&regexa, STORE_AXYZ_TO_PSEUDO, REG_EXTENDED) ||
        regcomp(&regexb, STORE_XY_TO_PSEUDO, REG_EXTENDED) ||
        regcomp(&regexc, STORE_A_TO_PSEUDO, REG_EXTENDED))
    {
        fprintf(stderr, "Could not compile regex\n");
        exit(EXIT_FAILURE);
    }

    int doopt;
    char *cursor;

    char snp_buf[MAXLEN_LINE];
    size_t i = 0;

    while (i < u)
    {
        if (IsControl(l[i]))
            printf("%s\n", l[i]);
        /*         if (StartsWith(l[i], "st"))
                {
                    if (!regexec(&regexa, l[i], maxGroups, groupArray, 0))
                    {
                        // printf("line %lu match: %s\n", i, l[i]);
                        //  printf("line=%lu line+30=%lu line_len=%u range=%lu->%u\n", i, (i + 30), u, (i + 1), FindMin(u, (i + 30)));
                        doopt = 0;
                        cursor = l[i];
                        char cursorCopy[strlen(cursor) + 1];
                        strcpy(cursorCopy, cursor);
                        cursorCopy[groupArray[2].rm_eo] = 0;
                        for (size_t j = (i + 1); j < FindMin(u, (i + 30)); j++)
                        {
                            // printf("line=%lu line+30=%lu line_len=%u range=%lu->%u\n", i, (i + 30), u, j, FindMin(u, (i + 30)));
                            snprintf(snp_buf, sizeof(snp_buf), "st([axyz]).b tcc__%s$", cursorCopy + groupArray[2].rm_so);
                            if (regcomp(&regexd,
                                        snp_buf,
                                        REG_EXTENDED))
                            {
                                fprintf(stderr, "Could not compile regex\n");
                                exit(EXIT_FAILURE);
                            }
                            if (!regexec(&regexd, l[j], maxGroups, groupArray, 0))
                            {
                                printf("CAS 1 %s\n", l[j]);
                                doopt = 1;
                                break;
                            }
                            if (StartsWith(l[j], "jsr.l") && !StartsWith(l[j], "jsr.l tcc__"))
                            {
                                printf("CAS 2 %s\n", l[j]);
                                doopt = 1;
                                break;
                            }
                            regfree(&regexd);
                        }
                        if (doopt)
                        {
                            ++i;
                            regfree(&regexd);
                            continue;
                        }
                    }
                } */
        ++i;
    }

    /* Free memory allocated to the pattern buffer by regcomp() */
    regfree(&regexa);
    regfree(&regexb);
    regfree(&regexc);
}

/* Main */
int main(int argc, char **argv)
{
    /* -------------------------------- */
    /*       Enable verbosity level     */
    /* -------------------------------- */
    size_t verbose = verbosity();
    if (verbose)
        fprintf(stderr, "Verbose mode is activated: %lu\n", verbose);

    /* -------------------------------- */
    /*       Store trimmed file         */
    /* -------------------------------- */
    DynArray file = TidyFile(argc, argv);

    if (verbose == 2)
    {
        for (size_t i = 0; i < file.used; i++)
        {
            // int z = 0;
            // if (strlen(file.arr[i]) == 0)
            // if (file.arr[i][0] == '\0')
            // z = 1;
            fprintf(stderr, "line[%6lu] : %s\n", i, file.arr[i]);
        }
        fprintf(stderr, "\n");
    }

    /* -------------------------------- */
    /*      Store BSS instuctions       */
    /* -------------------------------- */
    DynArray bss = StoreBss(file.arr, file.used);

    if (verbose == 2)
    {
        for (size_t i = 0; i < bss.used; i++)
        {
            fprintf(stderr, "line[%5lu] : %s\n", i, bss.arr[i]);
        }
        fprintf(stderr, "\n");
    }

    /* -------------------------------- */
    /*       ASM Optimization           */
    /* -------------------------------- */
    OptimizeAsm(file.arr, file.used);

    /* -------------------------------- */
    /*       Free pointers              */
    /* -------------------------------- */
    for (size_t i = 0; i < file.used; i++)
    {
        free(file.arr[i]);
    }

    for (size_t i = 0; i < bss.used; i++)
    {
        free(bss.arr[i]);
    }

    free(bss.arr);
    free(file.arr);
}