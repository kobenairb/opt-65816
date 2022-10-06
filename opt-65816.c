/*
 *  opt-65816 - Assembly code Optimizer for WDC65816
 *
 * Assembly code optimizer for WDC65816 processor produced by the 65816
 * Tiny C Compiler (816-tcc) developed by AlekMaul.
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
struct DArray
{
    int used;
    char **arr;
};

/* Create a dynamic string array to store
    block bss instructions (first word) */
struct DArray StoreBss(const int u, char **l)
{
    char **bss = NULL;
    size_t used = 0;
    int bss_on = 0;
    size_t nptrs = NPTRS;

    /* For strtok_r */
    char *saveptr;

    if ((bss = malloc(nptrs * sizeof *bss)) == NULL)
        CheckRc("malloc-lines", EXIT_FAILURE);

    for (int i = 0; i < u; i++)
    {
        size_t len;
        len = strlen(l[i]);

        if (StartWith(l[i], BSS_START))
        {
            bss_on = 1;
            continue;
        }
        if (StartWith(l[i], BSS_END) && bss_on)
        {
            bss_on = 0;
            continue;
        }
        if (!StartWith(l[i], BSS_START) && bss_on)
        {
            if (used == nptrs)
            { /* check if realloc of lines needed */
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
                CheckRc("malloc-lines[used]", 0);

            /* Store the first word of bss instruction */
            memcpy(bss[used], strtok_r(l[i], " ", &saveptr), len + 1);
            used += 1;
        }
    }

    struct DArray b = {used, bss};
    return b;
}

/* Create a dynamic string array from file
    without comment and leading/trailing white spaces */
struct DArray TidyFile(const int argc, char **argv)
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

        if (!StartWith(buf, COMMENT))
        {

            if (used == nptrs)
            { /* check if realloc of lines needed */
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

    struct DArray r = {used, lines};
    return r;
}

void optimize(const int u, char **l)
{
    /* total number of optimizations performed */
    // int totalopt = 0;
    /* have we optimized in this pass */
    // int opted = -1;
    /* optimization pass counter */
    // int opass = 0;
    regex_t regexa,
        regexb,
        regexc;

    int storetopseudo,
        storexytopseudo,
        storeatopseudo;

    storetopseudo = regcomp(&regexa, STORE_AXYZ_TO_PSEUDO, 0);
    storexytopseudo = regcomp(&regexb, STORE_XY_TO_PSEUDO, 0);
    storeatopseudo = regcomp(&regexc, STORE_A_TO_PSEUDO, 0);

    if (storetopseudo ||
        storexytopseudo ||
        storeatopseudo)
    {
        fprintf(stderr, "Could not compile regex\n");
        exit(1);
    }

    size_t i = 0;

    printf("number of line : %d\n", u);
    while (i < u)
    {
        if (StartWith(l[i], "st"))
        {
            /* Execute regular expression */
            storetopseudo = regexec(&regexa, l[i], (size_t)0, NULL, 0);
            // if (!storetopseudo)
            //  printf("line %ld match: %s\n", i, l[i]);
            //  printf("line=%ld line+30=%ld line_len=%d range=%ld->%d\n", i, i + 30, u, i + 1, findSmall(u, i + 30));
        }
        ++i;
    }
    /* Free memory allocated to the pattern buffer by regcomp() */
    regfree(&regexa);
    regfree(&regexb);
    regfree(&regexc);
}

/* ---------- */
/*    Main    */
/* ---------- */
int main(int argc, char **argv)
{
    /* Enable verbosity level */
    int verbose = verbosity();
    if (verbose)
        fprintf(stderr, "Verbose mode is activated: %d\n", verbose);

    /* -------------------------------- */
    /*       store trimmed file         */
    /* -------------------------------- */
    struct DArray f = TidyFile(argc, argv);

    if (verbose == 2)
    {
        for (int i = 0; i < f.used; i++)
        {
            fprintf(stderr, "line[%6u] : %s\n", i, f.arr[i]);
        }
        fprintf(stderr, "\n");
    }

    /* -------------------------------- */
    /*      store BSS instuctions       */
    /* -------------------------------- */
    struct DArray b = StoreBss(f.used, f.arr);

    if (verbose == 2)
    {
        for (int i = 0; i < b.used; i++)
        {
            printf("line[%5u] : %s\n", i, b.arr[i]);
        }
        fprintf(stderr, "\n");
    }

    /* -------------------------------- */
    /*           optimization           */
    /* -------------------------------- */
    optimize(f.used, f.arr);
    /* -------------------------------- */
    /*    free pointers of pointers     */
    /* -------------------------------- */
    for (int i = 0; i < f.used; i++)
    {
        free(f.arr[i]);
    }

    for (int i = 0; i < b.used; i++)
    {
        free(b.arr[i]);
    }

    /* -------------------------------- */
    /*          free pointers           */
    /* -------------------------------- */
    free(b.arr);
    free(f.arr);
}