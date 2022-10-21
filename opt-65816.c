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
 * @param text The array of strings.
 * @param n The number of elements in the array.
 * @return A structure (DynArray).
 */
DynArray StoreBss(char **text, const size_t n)
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
        if (StartsWith(text[i], BSS_START))
        {
            bss_on = 1;
            continue;
        }
        if (StartsWith(text[i], BSS_END) && bss_on)
        {
            bss_on = 0;
            continue;
        }
        if (!StartsWith(text[i], BSS_START) && bss_on)
        {
            bss[used] = malloc(strlen(text[i]) + 1);
            memcpy(bss[used], text[i], strlen(text[i]) + 1);
            strtok_r(bss[used], " ", &saveptr);
            used += 1;
        }
    }

    DynArray b = {bss, used};
    return b;
}

/**
 * @brief Optimize ASM code.
 * @param text
 * @param n
 */
void OptimizeAsm(char **text, const size_t n)
{
    /* total number of optimizations performed */
    // int totalopt = 0;
    /* have we OptimizeAsmd in this pass */
    int opted = -1;
    /* optimization pass counter */
    // int opass = 0;

    RegDynArray r, r1;

    size_t doopt;

    size_t cont;

    char crem[2][4] = {"inc", "dec"};

    /* some char to handle snprintf buffers */
    char snp_buf1[MAXLEN_LINE],
        snp_buf2[MAXLEN_LINE];

    /* Manage pointers */
    char **text_opt = NULL;
    size_t used = 0;
    size_t nptrs = n;

    if ((text_opt = malloc(nptrs * sizeof *text_opt)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    size_t i = 0;

    while (i < n)
    {
        if (StartsWith(text[i], "st"))
        {
            /* eliminate redundant stores */
            r = RegMatchGroups(text[i], STORE_AXYZ_TO_PSEUDO, 3);
            if (r.status == 1)
            {
                doopt = 0;
                for (size_t j = (i + 1); j < (size_t)FindMin(n, (i + 30)); j++)
                {
                    snprintf(snp_buf1, sizeof(snp_buf1), "st([axyz]).b tcc__%s$", r.groups[2]);
                    r1 = RegMatchGroups(text[j], snp_buf1, 2);
                    if (r1.status == 1)
                    {
                        printf("[CAS 1] %lu: %s\n", i, text[j]);
                        FreeDynArray(r1.groups, r1.used);

                        doopt = 1;
                        break;
                    }
                    /* Before function call (will be clobbered anyway) */
                    if (StartsWith(text[j], "jsr.l ") && !StartsWith(text[j], "jsr.l tcc__"))
                    {
                        printf("[CAS 2] %lu: %s\n", i, text[j]);
                        doopt = 1;
                        break;
                    }
                    /* Cases in which we don't pursue optimization further */
                    /* #1 Branch or other use of the pseudo register */
                    snprintf(snp_buf1, sizeof(snp_buf1), "tcc__%s", r.groups[2]);
                    if (IsControl(text[j]) || IsInText(text[j], snp_buf1))
                    {
                        printf("[CAS 3] %lu: %s\n", i, text[j]);
                        break;
                    }
                    /* #2 Use as a pointer */
                    snprintf(snp_buf1, sizeof(snp_buf1), "[tcc__%s", r.groups[2]);
                    snp_buf1[strlen(snp_buf1) - 1] = '\0'; // Remove the last char
                    if (EndsWith(r.groups[2], "h") && IsInText(text[j], snp_buf1))
                    {
                        printf("[CAS 4] %lu: %s\n", i, text[j]);
                        break;
                    }
                }
                FreeDynArray(r.groups, r.used);
                if (doopt)
                {
                    /* Skip redundant store */
                    i += 1;
                    opted += 1;
                    continue;
                }
            }
            /* Stores (x/y) to pseudo-registers */
            r = RegMatchGroups(text[i], STORE_XY_TO_PSEUDO, 3);
            if (r.status == 1)
            {
                /* Store hwreg to preg, push preg,
                    function call -> push hwreg, function call */
                snprintf(snp_buf1, sizeof(snp_buf1), "pei (tcc__%s)", r.groups[2]);
                if (strcmp(text[i + 1], snp_buf1) == 0 &&
                    StartsWith(text[i + 2], "jsr.l "))
                {
                    printf("[CAS 5] %lu: %s\n", i + 1, text[i + 1]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "ph%s", r.groups[1]);
                    text_opt[used] = malloc(strlen(snp_buf1) + 1);
                    memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                    used += 1;

                    FreeDynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store hwreg to preg, push preg -> store hwreg to preg,
                    push hwreg (shorter) */
                if (strcmp(text[i + 1], snp_buf1) == 0)
                {
                    printf("[CAS 6] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt[used] = malloc(strlen(text[i]) + 1);
                    memcpy(text_opt[used], text[i], strlen(text[i]) + 1);
                    used += 1;
                    snprintf(snp_buf1, sizeof(snp_buf1), "ph%s", r.groups[1]);
                    text_opt[used] = malloc(strlen(snp_buf1) + 1);
                    memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                    used += 1;

                    FreeDynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store hwreg to preg, load hwreg from preg -> store hwreg to preg,
                    transfer hwreg/hwreg (shorter) */
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s", r.groups[2]);
                snprintf(snp_buf2, sizeof(snp_buf2), "lda.b tcc__%s ; DON'T OPTIMIZE", r.groups[2]);
                if (strcmp(text[i + 1], snp_buf1) == 0 ||
                    strcmp(text[i + 1], snp_buf2) == 0)
                {
                    printf("[CAS 7] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt[used] = malloc(strlen(text[i]) + 1);
                    memcpy(text_opt[used], text[i], strlen(text[i]) + 1);
                    used += 1;
                    snprintf(snp_buf1, sizeof(snp_buf1),
                             "t%sa", r.groups[1]); // FIXME: shouldn't this be marked as
                                                   // DON'T OPTIMIZE again?
                    text_opt[used] = malloc(strlen(snp_buf1) + 1);
                    memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                    used += 1;

                    FreeDynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                FreeDynArray(r.groups, r.used);
            }
            /* Stores (accu only) to pseudo-registers */
            r = RegMatchGroups(text[i], STORE_A_TO_PSEUDO, 2);
            if (r.status == 1)
            {
                /* Store preg followed by load preg */
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s", r.groups[1]);
                if (strcmp(text[i + 1], snp_buf1) == 0)
                {
                    printf("[CAS 8] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt[used] = malloc(strlen(text[i]) + 1);
                    memcpy(text_opt[used], text[i],
                           strlen(text[i]) + 1); // Keep store
                    used += 1;

                    FreeDynArray(r.groups, r.used);

                    i += 2; // Omit load
                    opted += 1;
                    continue;
                }
                /* Store preg followed by load preg with ldx/ldy in between */
                if ((StartsWith(text[i + 1], "ldx") ||
                     StartsWith(text[i + 1], "ldy")) &&
                    strcmp(text[i + 2], snp_buf1) == 0)
                {
                    printf("[CAS 9] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt[used] = malloc(strlen(text[i]) + 1);
                    memcpy(text_opt[used], text[i],
                           strlen(text[i]) + 1); // Keep store
                    used += 1;
                    text_opt[used] = malloc(strlen(text[i + 1]) + 1);
                    memcpy(text_opt[used], text[i + 1], strlen(text[i + 1]) + 1);
                    used += 1;

                    FreeDynArray(r.groups, r.used);

                    i += 3; // Omit load
                    opted += 1;
                    continue;
                }
                /* Store accu to preg, push preg, function call -> push accu,
                    function call */
                snprintf(snp_buf1, sizeof(snp_buf1), "pei (tcc__%s)", r.groups[1]);
                if (strcmp(text[i + 1], snp_buf1) == 0 && StartsWith(text[i + 2], "jsr.l "))
                {
                    printf("[CAS 10] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt[used] = malloc(strlen("pha") + 1);
                    memcpy(text_opt[used], "pha", strlen("pha") + 1);
                    used += 1;

                    FreeDynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store accu to preg, push preg -> store accu to preg,
                    push accu (shorter) */
                if (strcmp(text[i + 1], snp_buf1) == 0)
                {
                    printf("[CAS 11] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt[used] = malloc(strlen(text[i + 1]) + 1);
                    memcpy(text_opt[used], text[i + 1], strlen(text[i + 1]) + 1);
                    used += 1;
                    text_opt[used] = malloc(strlen("pha") + 1);
                    memcpy(text_opt[used], "pha", strlen("pha") + 1);
                    used += 1;

                    FreeDynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store accu to preg1, push preg2, push preg1 -> store accu to preg1,
                    push preg2, push accu */
                else if (StartsWith(text[i + 1], "pei ") && strcmp(text[i + 2], snp_buf1) == 0)
                {
                    printf("[CAS 12] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt[used] = malloc(strlen(text[i + 1]) + 1);
                    memcpy(text_opt[used], text[i + 1], strlen(text[i + 1]) + 1);
                    used += 1;
                    text_opt[used] = malloc(strlen(text[i]) + 1);
                    memcpy(text_opt[used], text[i], strlen(text[i]) + 1);
                    used += 1;

                    FreeDynArray(r.groups, r.used);

                    i += 3;
                    opted += 1;
                    continue;
                }
                /* Convert incs/decs on pregs incs/decs on hwregs */
                cont = 0;
                for (size_t k = 0; k < sizeof(crem) / sizeof(crem[0]); k++)
                {
                    snprintf(snp_buf1, sizeof(snp_buf1), "%s.b tcc__%s", crem[k], r.groups[1]);
                    if (strcmp(text[i + 1], snp_buf1) == 0)
                    {
                        printf("[CAS 13] %lu: %s\n", i + 1, text[i + 1]);
                        /* Store to preg followed by crement on preg */
                        if (strcmp(text[i + 2], snp_buf1) == 0 && StartsWith(text[i + 3], "lda"))
                        {
                            printf("[CAS 14] %lu: %s\n", i + 2, text[i + 2]);

                            /* Store to preg followed by two crements on preg
                                increment the accu first, then store it to preg */
                            snprintf(snp_buf1, sizeof(snp_buf1), "%s a", crem[k]);
                            text_opt[used] = malloc(strlen(snp_buf1) + 1);
                            memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                            used += 1;
                            text_opt[used] = malloc(strlen(snp_buf1) + 1);
                            memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                            used += 1;
                            snprintf(snp_buf1, sizeof(snp_buf1), "sta.b tcc__%s", r.groups[1]);
                            text_opt[used] = malloc(strlen(snp_buf1) + 1);
                            memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                            used += 1;

                            /* A subsequent load can be omitted (the right value is already in the accu) */
                            snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s", r.groups[1]);
                            if (strcmp(text[i + 3], snp_buf1) == 0)
                            {
                                printf("[CAS 15] %lu: %s\n", i + 3, text[i + 3]);
                                i += 4;
                            }
                            else
                            {
                                printf("[CAS 16] %lu: %s\n", i, text[i]);
                                i += 3;
                            }
                            FreeDynArray(r.groups, r.used);

                            opted += 1;
                            cont += 1;
                            break;
                        }
                        else if (StartsWith(text[i + 2], "lda"))
                        {
                            printf("[CAS 17] %lu: %s\n", i + 2, text[i + 2]);

                            snprintf(snp_buf1, sizeof(snp_buf1), "%s a", crem[k]);
                            text_opt[used] = malloc(strlen(snp_buf1) + 1);
                            memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                            used += 1;
                            snprintf(snp_buf1, sizeof(snp_buf1), "sta.b tcc__%s", r.groups[1]);
                            text_opt[used] = malloc(strlen(snp_buf1) + 1);
                            memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                            used += 1;

                            snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s", r.groups[1]);
                            if (strcmp(text[i + 2], snp_buf1) == 0)
                            {
                                printf("[CAS 18] %lu: %s\n", i + 2, text[i + 2]);
                                i += 3;
                            }
                            else
                            {
                                printf("[CAS 19] %lu: %s\n", i, text[i]);
                                i += 2;
                            }

                            FreeDynArray(r.groups, r.used);

                            opted += 1;
                            cont += 1;
                            break;
                        }
                    }
                }
                if (cont)
                    continue;

                r1 = RegMatchGroups(text[i + 1], "lda.b tcc__([rf][0-9]{0,})", 2);
                if (r1.status == 1)
                {
                    printf("[CAS 20] %lu: %s\n", i + 1, text[i + 1]);
                    if (StartsWith(text[i + 2], "and") || StartsWith(text[i + 2], "ora"))
                    {
                        printf("[CAS 21] %lu: %s\n", i + 2, text[i + 2]);
                        snprintf(snp_buf1, sizeof(snp_buf1), ".b tcc__%s", r.groups[1]);
                        /* Store to preg1, load from preg2, and/or preg1 -> store to preg1, and/or preg2 */
                        if (EndsWith(text[i + 2], snp_buf1))
                        {
                            printf("[CAS 22] %lu: %s\n", i + 2, text[i + 2]);

                            text_opt[used] = malloc(strlen(text[i]) + 1);
                            memcpy(text_opt[used], text[i], strlen(text[i]) + 1);
                            used += 1;
                            snprintf(snp_buf1, sizeof(snp_buf1), "%.3s.b tcc__%s", text[i + 2], r1.groups[1]);
                            text_opt[used] = malloc(strlen(snp_buf1) + 1);
                            memcpy(text_opt[used], snp_buf1, strlen(snp_buf1) + 1);
                            used += 1;
                            FreeDynArray(r1.groups, r1.used);

                            i += 3;
                            opted += 1;
                            continue;
                        }
                    }
                    FreeDynArray(r1.groups, r1.used);
                }
                /* Store to preg, switch to 8 bits, load from preg => skip the load */
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s", r.groups[1]);
                if (strcmp(text[i + 1], "sep #$20") == 0 &&
                    strcmp(text[i + 2], snp_buf1) == 0)
                {
                    printf("[CAS 23] %lu: %s\n", i + 2, text[i + 2]);

                    text_opt[used] = malloc(strlen(text[i]) + 1);
                    memcpy(text_opt[used], text[i], strlen(text[i]) + 1);
                    used += 1;
                    text_opt[used] = malloc(strlen(text[i + 1]) + 1);
                    memcpy(text_opt[used], text[i + 1], strlen(text[i + 1]) + 1);
                    used += 1;
                    FreeDynArray(r.groups, r.used);

                    i += 3; // Skip load
                    opted += 1;
                    continue;
                }

                /* Two stores to preg without control flow or other uses of preg => skip first store */
                snprintf(snp_buf1, sizeof(snp_buf1), "tcc__%s", r.groups[1]);
                if (IsControl(text[i + 1]) == 0 &&
                    IsInText(text[i + 1], snp_buf1) == 0)
                {
                    printf("[CAS 24] %lu: %s\n", i + 1, text[i + 1]);
                    if (strcmp(text[i + 2], text[i]) == 0)
                    {
                        printf("[CAS 25] %lu: %s\n", i + 2, text[i + 2]);

                        text_opt[used] = malloc(strlen(text[i + 1]) + 1);
                        memcpy(text_opt[used], text[i + 1], strlen(text[i + 1]) + 1);
                        used += 1;
                        text_opt[used] = malloc(strlen(text[i + 2]) + 1);
                        memcpy(text_opt[used], text[i + 2], strlen(text[i + 2]) + 1);
                        used += 1;

                        FreeDynArray(r.groups, r.used);

                        i += 3; // Skip first store
                        opted += 1;
                        continue;
                    }
                }

                FreeDynArray(r.groups, r.used);
            }
        }
        i++;
    }

    printf("\n\n______________[ASM CODE]_________________\n");
    for (size_t i = 0; i < used; i++)
    {
        printf("%s\n", text_opt[i]);
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
            fprintf(stderr, "line[%6lu] : %s\n", i, file.text[i]);
        }
        fprintf(stderr, "\n");
    }

    /* -------------------------------- */
    /*      Store BSS instuctions       */
    /* -------------------------------- */
    DynArray bss = StoreBss(file.text, file.used);

    if (verbose == 2)
    {
        for (size_t i = 0; i < bss.used; i++)
        {
            fprintf(stderr, "line[%5lu] : %s\n", i, bss.text[i]);
        }
        fprintf(stderr, "\n");
    }

    /* -------------------------------- */
    /*       ASM Optimization           */
    /* -------------------------------- */
    OptimizeAsm(file.text, file.used);

    /* -------------------------------- */
    /*       Free pointers              */
    /* -------------------------------- */
    FreeDynArray(bss.text, bss.used);
    FreeDynArray(file.text, file.used);
}