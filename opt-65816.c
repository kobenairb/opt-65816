/*
 * opt-65816 - Assembly code optimizer for WDC65816.
 *
 * Description: Assembly code optimizer for the WDC65816 processor produced
 * by the 65816 Tiny C Compiler (816-tcc) developed by AlekMaul.
 * This library is a C port of the 816-opt python tool developed by nArnoSNES.
 *
 * Author: kobenairb (kobenairb@gmail.com).
 *
 * Copyright (c) 2022.
 *
 * This project is released under the GNU Public License.
 *
 */

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

/**
 * @brief Create an array of strings from a file
    without comment and leading/trailing white spaces.
    Accept an ASM file as argument or stdin.
 * @param argc The number of arguments provided.
 * @param argv The arguments provided.
 * @return A structure (DynArray).
 */
DynArray TidyFile(const int argc, char **argv)
{
    char buf[MAXLEN_LINE];
    char **lines = NULL;
    size_t nptrs = 1;
    size_t used = 0;
    size_t len;

    if (argc > 2)
    {
        printf("usage:\n");
        printf("  - %s <filename>\n", argv[0]);
        printf("  - <stdin> | %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE *fp = argc > 1 ? fopen(argv[1], "r") : stdin;

    if (!fp)
    {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }

    if ((lines = malloc(nptrs * sizeof *lines)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    while (fgets(buf, MAXLEN_LINE, fp))
    {
        buf[(len = strcspn(buf, "\n"))] = 0;

        if (!StartsWith(buf, COMMENT))
        {
            if (used == nptrs)
            {
                void *tmp = realloc(lines, (2 * nptrs) * sizeof *lines);
                if (!tmp)
                {
                    perror("realloc-lines");
                    break;
                }
                lines = tmp;
                nptrs *= 2;
            }
            if (!(lines[used] = malloc(len + 1)))
            {
                perror("malloc-lines[used]");
                break;
            }
            memcpy(lines[used], TrimWhiteSpace(buf), len + 1);
            used += 1;
        }
    }
    if (fp != stdin)
        fclose(fp);

    DynArray r = {lines, used};
    return r;
}

/**
 * @brief Create an array of string to store
    block bss instructions (first word only).
 * @param arr The array of strings.
 * @param n The number of elements in the array.
 * @return A structure (DynArray).
 */
DynArray StoreBss(char **arr, const size_t n)
{
    char **bss = NULL;
    char *saveptr;

    size_t used = 0;
    size_t bss_on = 0;
    size_t nptrs = n;

    if ((bss = malloc(nptrs * sizeof *bss)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < n; i++)
    {
        if (StartsWith(arr[i], BSS_START))
        {
            bss_on = 1;
            continue;
        }
        if (StartsWith(arr[i], BSS_END) && bss_on)
        {
            bss_on = 0;
            continue;
        }
        if (!StartsWith(arr[i], BSS_START) && bss_on)
        {
            bss[used] = malloc(strlen(arr[i]) + 1);
            memcpy(bss[used], strtok_r(arr[i], " ", &saveptr), strlen(arr[i]) + 1);
            used += 1;
        }
    }

    DynArray b = {bss, used};
    return b;
}

/**
 * @brief Optimize ASM code.
 * @param arr
 * @param u
 */
void OptimizeAsm(char **arr, const size_t u)
{
    /* total number of optimizations performed */
    // int totalopt = 0;
    /* have we OptimizeAsmd in this pass */
    int opted = -1;
    /* optimization pass counter */
    // int opass = 0;

    size_t maxGroups = 3;

    regex_t regexd;

    RegDynArray r;

    size_t doopt;

    /* some char to handle snprintf buffers */
    char snp_buf1[MAXLEN_LINE],
        snp_buf2[MAXLEN_LINE];

    /* Manage pointers */
    char **text_opt = NULL;
    size_t used = 0;
    size_t nptrs = u;

    if ((text_opt = malloc(nptrs * sizeof *text_opt)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    size_t i = 0;

    while (i < u)
    {
        if (StartsWith(arr[i], "st"))
        {
            /* eliminate redundant stores */
            r = RegMatchGroups(arr[i], STORE_AXYZ_TO_PSEUDO, maxGroups);
            regfree(&r.regexCompiled);
            if (r.status)
            {
                doopt = 0;
                for (size_t j = (i + 1); j < (size_t)FindMin(u, (i + 30)); j++)
                {
                    snprintf(snp_buf1, sizeof(snp_buf1), "st([axyz]).b tcc__%s$", r.groups[2]);
                    if (regcomp(&regexd,
                                snp_buf1,
                                0))
                    {
                        fprintf(stderr, "Could not compile regex\n");
                        exit(EXIT_FAILURE);
                    }
                    /* Another store to the same pregister */
                    if (!regexec(&regexd, arr[j], 0, NULL, 0))
                    {
                        printf("[CAS 1] %lu: %s\n", i, arr[j]);
                        doopt = 1;
                        break;
                    }
                    /* Before function call (will be clobbered anyway) */
                    if (StartsWith(arr[j], "jsr.l ") && !StartsWith(arr[j], "jsr.l tcc__"))
                    {
                        printf("[CAS 2] %lu: %s\n", i, arr[j]);
                        doopt = 1;
                        break;
                    }
                    /* Cases in which we don't pursue optimization further */
                    /* #1 Branch or other use of the pseudo register */
                    snprintf(snp_buf1, sizeof(snp_buf1), "tcc__%s", r.groups[2]);
                    if (IsControl(arr[j]) || IsInText(arr[j], snp_buf1))
                    {
                        printf("[CAS 3] %lu: %s\n", i, arr[j]);
                        break;
                    }
                    /* #2 Use as a pointer */
                    snprintf(snp_buf1, sizeof(snp_buf1), "[tcc__%s", r.groups[2]);
                    snp_buf1[strlen(snp_buf1) - 1] = '\0';
                    if (EndsWith(r.groups[2], "h") && IsInText(arr[j], snp_buf1))
                    {
                        printf("[CAS 4] %lu: %s\n", i, arr[j]);
                        break;
                    }
                    regfree(&regexd);
                }
                regfree(&regexd);
                FreeDynArray(r.groups, maxGroups);
                if (doopt)
                {
                    /* Skip redundant store */
                    i += 1;
                    opted += 1;
                    continue;
                }
            }
            /* Stores (x/y) to pseudo-registers */
            r = RegMatchGroups(arr[i], STORE_XY_TO_PSEUDO, maxGroups);
            regfree(&r.regexCompiled);
            if (r.status)
            {
                /* Store hwreg to preg, push preg,
                    function call -> push hwreg, function call */
                snprintf(snp_buf1, sizeof(snp_buf1), "pei (tcc__%s)", r.groups[2]);
                if (StartsWith(arr[i + 1], snp_buf1) && StartsWith(arr[i + 2], "jsr.l "))
                {
                    printf("[CAS 5] %lu: %s\n", i + 1, arr[i + 1]);
                    snprintf(snp_buf1, sizeof(snp_buf1), "ph%s", r.groups[1]);
                    text_opt[used] = malloc(strlen(snp_buf1) + 1);
                    memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                    used += 1;
                    i += 2;
                    opted += 1;
                    FreeDynArray(r.groups, maxGroups);
                    continue;
                }
                /* Store hwreg to preg, push preg -> store hwreg to preg,
                    push hwreg (shorter) */
                if (StartsWith(arr[i + 1], snp_buf1))
                {
                    printf("[CAS 6] %lu: %s\n", i + 1, arr[i + 1]);
                    text_opt[used] = malloc(strlen(arr[i]) + 1);
                    memcpy(text_opt[used], arr[i], strlen(arr[i]) + 1);
                    used += 1;
                    snprintf(snp_buf1, sizeof(snp_buf1), "ph%s", r.groups[1]);
                    text_opt[used] = malloc(strlen(snp_buf1) + 1);
                    memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                    used += 1;
                    i += 2;
                    opted += 1;
                    FreeDynArray(r.groups, maxGroups);
                    continue;
                }
                /* Store hwreg to preg, load hwreg from preg -> store hwreg to preg,
                    transfer hwreg/hwreg (shorter) */
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s", r.groups[2]);
                snprintf(snp_buf2, sizeof(snp_buf2), "lda.b tcc__%s ; DON'T OPTIMIZE", r.groups[2]);
                if (StartsWith(arr[i + 1], snp_buf1) || StartsWith(arr[i + 1], snp_buf2))
                {
                    printf("[CAS 7] %lu: %s\n", i + 1, arr[i + 1]);
                    text_opt[used] = malloc(strlen(arr[i]) + 1);
                    memcpy(text_opt[used], arr[i], strlen(arr[i]) + 1);
                    used += 1;
                    snprintf(snp_buf1, sizeof(snp_buf1), "t%sa", r.groups[1]);
                    text_opt[used] = malloc(strlen(snp_buf1) + 1);
                    memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                    used += 1;
                    i += 2;
                    opted += 1;
                    FreeDynArray(r.groups, maxGroups);
                    continue;
                }
                FreeDynArray(r.groups, maxGroups);
            }
        }
        i++;
    }
    FreeDynArray(text_opt, used);
}

/**
 * @brief The main function. Accept an ASM file
 as argument or stdin.
 * @param argc The number of arguments provided.
 * @param argv The arguments provided.
 * @return 0
 */
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

    // if (verbose == 2)
    // {
    //     for (size_t i = 0; i < used; i++)
    //     {
    //         printf("%s\n", text_opt[i]);
    //     }
    // }

    /* -------------------------------- */
    /*       Free pointers              */
    /* -------------------------------- */
    FreeDynArray(tmp.arr, tmp.used);
    FreeDynArray(bss.arr, bss.used);
    FreeDynArray(file.arr, file.used);
}