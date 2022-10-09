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

#include "opt-65816.h"
#include "libopt-65816.c"

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

    if (argc > 2)
    {
        printf("usage:\n");
        printf("  - %s <filename>\n", argv[0]);
        printf("  - <stdin> | %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* use filename provided as 1st argument (stdin by default) */
    FILE *fp = argc > 1 ? fopen(argv[1], "r") : stdin;

    /* validate file open for reading */
    if (!fp)
    {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }

    /* allocate/validate block holding initial nptrs pointers */
    if ((lines = malloc(nptrs * sizeof *lines)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

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
                    perror("realloc-lines");
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
                perror("malloc-lines[used]");
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
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

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
                    perror("realloc-lines");
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
                perror("malloc-lines[used]");
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
void OptimizeAsm(char **l, const int u)
{
    /* total number of optimizations performed */
    // int totalopt = 0;
    /* have we OptimizeAsmd in this pass */
    int opted = -1;
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
        ChangesAccu(l[i]);

        if (StartsWith(l[i], "st"))
        {
            /* eliminate redundant stores */
            if (!regexec(&regexa, l[i], maxGroups, groupArray, 0))
            {
                doopt = 0;
                cursor = l[i];
                char cursorCopy[strlen(cursor) + 1];
                strcpy(cursorCopy, cursor);
                cursorCopy[groupArray[2].rm_eo] = 0;
                for (size_t j = (i + 1); j < FindMin(u, (i + 30)); j++)
                {
                    snprintf(snp_buf, sizeof(snp_buf), "st([axyz]).b tcc__%s$", cursorCopy + groupArray[2].rm_so);
                    if (regcomp(&regexd,
                                snp_buf,
                                REG_EXTENDED))
                    {
                        fprintf(stderr, "Could not compile regex\n");
                        exit(EXIT_FAILURE);
                    }
                    /* Another store to the same pregister */
                    if (!regexec(&regexd, l[j], maxGroups, groupArray, 0))
                    {
                        printf("[CAS 1] %lu: %s\n", i, l[j]);
                        doopt = 1;
                        break;
                    }
                    /* Before function call (will be clobbered anyway) */
                    if (StartsWith(l[j], "jsr.l ") && !StartsWith(l[j], "jsr.l tcc__"))
                    {
                        printf("[CAS 2] %lu: %s\n", i, l[j]);
                        doopt = 1;
                        break;
                    }
                    /* Cases in which we don't pursue optimization further */
                    /* #1 Branch or other use of the pseudo register */
                    snprintf(snp_buf, sizeof(snp_buf), "tcc__%s", cursorCopy + groupArray[2].rm_so);
                    if (IsControl(l[j]) || IsInText(l[j], snp_buf))
                    {
                        printf("[CAS 3] %lu: %s\n", i, l[j]);
                        break;
                    }
                    /* #2 Use as a pointer */
                    snprintf(snp_buf, sizeof(snp_buf), "[tcc__%s", cursorCopy + groupArray[2].rm_so);
                    snp_buf[strlen(snp_buf) - 1] = '\0';
                    if (EndsWith(cursorCopy + groupArray[2].rm_so, "h") && IsInText(l[j], snp_buf))
                    {
                        printf("[CAS 4] %lu: %s\n", i, l[j]);
                        break;
                    }
                    regfree(&regexd);
                }
                regfree(&regexd);
                if (doopt)
                {
                    ++i;
                    ++opted;
                    continue;
                }
            }
        }
        i++;
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
            fprintf(stderr, "line[%6lu] : %s\n", i, file.arr[i]);
        }
        fprintf(stderr, "\n");
    }

    /* -------------------------------- */
    /*      Store BSS instuctions       */
    /* -------------------------------- */
    /* Too lazy to write a function to copy
        file.arr. TODO */
    DynArray tmp = TidyFile(argc, argv);
    DynArray bss = StoreBss(tmp.arr, file.used);

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
    FreeDynArray(tmp.arr, tmp.used);
    FreeDynArray(bss.arr, bss.used);
    FreeDynArray(file.arr, file.used);
}