/*
 *  opt-65816 - ASM Optimizer for WDC65815
 *
 * ASM code optimizer for WDC65816 processor produced by the 65816 TCC
 * developed by AlekMaul. This library is a C port of the 816-opt python
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#endif

#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#endif

#include "opt-65816.h"

/* Structure to store an array
    and the number of elements */
struct DArray
{
    int used;
    char **arr;
};

/* Enable verbosity if OPT_816_QUIET is set
    OPT_816_QUIET=0 or unset (no verbosity)
    OPT_816_QUIET=1          (normal verbosity)
    OPT_816_QUIET=2          (for debug purpose) */
int verbosity()
{
    char *OPT_816_QUIET = getenv("OPT_816_QUIET");

    if (!OPT_816_QUIET || *OPT_816_QUIET == '0')
        return 0;
    else if (OPT_816_QUIET && *OPT_816_QUIET == '1')
        return 1;
    else if (OPT_816_QUIET && *OPT_816_QUIET == '2')
        return 2;

    return 3;
}

/* Check if a string starts with */
int start_with(const char *a, const char *b)
{
    if (strncmp(a, b, strlen(b)) == 0)
        return 1;

    return 0;
}

/* Check return code */
void check_rc(const char *message, const int rc)
{
    if (rc)
    {
        perror(message);
        exit(rc);
    }

    perror(message);
}

/* Create a dynamic string array to store
    block bss instructions */
struct DArray store_bss(const int u, char **l)
{
    char **bss = NULL;
    size_t used = 0;
    int bss_on = 0;
    size_t nptrs = NPTRS;

    /* For strtok_r */
    char *saveptr;

    if ((bss = malloc(nptrs * sizeof *bss)) == NULL)
        check_rc("malloc-lines", EXIT_FAILURE);

    for (int i = 0; i < u; i++)
    {
        size_t len;
        len = strlen(l[i]);

        if (start_with(l[i], BSS_START))
        {
            bss_on = 1;
            continue;
        }
        if (start_with(l[i], BSS_END) && bss_on)
        {
            bss_on = 0;
            continue;
        }
        if (!start_with(l[i], BSS_START) && bss_on)
        {
            if (used == nptrs)
            { /* check if realloc of lines needed */
                /* always realloc using temporary pointer (doubling no. of pointers) */
                void *tmp = realloc(bss, (2 * nptrs) * sizeof *bss);
                if (!tmp)
                {
                    check_rc("realloc-lines", 0);
                    break;
                }
                /* assign reallocated block to lines */
                bss = tmp;
                /* update no. of pointers allocatd */
                nptrs *= 2;
            }

            /* allocate/validate storage for line */
            if (!(bss[used] = malloc(len + 1)))
                check_rc("malloc-lines[used]", 0);

            /* Store the first word of bss instruction */
            memcpy(bss[used], strtok_r(l[i], " ", &saveptr), len + 1);
            used += 1;
        }
    }

    struct DArray b = {used, bss};
    return b;
}

/* Create a dynamic string array from file
    without comment nor empty line */
struct DArray trim_file(const int argc, char **argv)
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
        check_rc("file open failed", EXIT_FAILURE);

    /* allocate/validate block holding initial nptrs pointers */
    if ((lines = malloc(nptrs * sizeof *lines)) == NULL)
        check_rc("malloc-lines", EXIT_FAILURE);

    while (fgets(buf, MAXLEN_LINE, fp))
    { /* read each line into buf */
        size_t len;
        buf[(len = strcspn(buf, "\n"))] = 0; /* trim \n, save length */

        if (!start_with(buf, COMMENT) && buf[0] != '\0')
        {

            if (used == nptrs)
            { /* check if realloc of lines needed */
                /* always realloc using temporary pointer (doubling no. of pointers) */
                void *tmp = realloc(lines, (2 * nptrs) * sizeof *lines);
                if (!tmp)
                {
                    check_rc("realloc-lines", 0);
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
                check_rc("malloc-lines[used]", 0);
                break;
            }
            /* copy line from buf to lines[used] */
            memcpy(lines[used], buf, len + 1);
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

void optimize()
{
    /* total number of optimizations performed */
    // int totalopt = 0;
    /* have we optimized in this pass */
    int opted = -1;
    /* optimization pass counter */
    // int opass = 0;

    if (opted)
        fprintf(stderr, "Not optimized yet\n");
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

    /* argc must be 2 for correct execution */
    if (argc != 2)
    {
        printf("usage: %s <file_name>\n", argv[0]);
        check_rc("Wrong argument", EXIT_FAILURE);
    }

    /* -------------------------------- */
    /*       store trimmed file         */
    /* -------------------------------- */
    struct DArray f = trim_file(argc, argv);

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
    struct DArray b = store_bss(f.used, f.arr);

    if (verbose == 2)
    {
        for (int i = 0; i < b.used; i++)
        {
            printf("line[%5u] : %s\n", i, b.arr[i]);
        }
        fprintf(stderr, "\n");
    }

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

    optimize();
}