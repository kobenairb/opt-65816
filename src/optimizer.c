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

#include "optimizer.h"

/**
 * @brief Checks if 816TCCOPT_QUIET is set.
 * This environment variable sets the output in a quiet mode.
 * Just set it if you don't want extra messages.
 * @return 0 (verbose), 1 (quiet).
 */
int verbosity()
{
    char *QUIET_MODE = getenv("OPT816_QUIET");

    if (!QUIET_MODE)
        return 1;
    return 0;
}

/**
 * @brief Checks if it touches the accumulator register.
 * @param a The asm instruction.
 * @return 1 (true) or 0 (false).
 */
int changeAccu(const char *a)
{
    if (strlen(a) > 2)
    {
        if (a[2] == 'a'
            && (!startWith(a, "pha") || !startWith(a, "sta")))
            return 1;
        if (strlen(a) == 5 && endWith(a, " a"))
            return 1;
    }

    return 0;
}

/**
 * @brief Check if the line alters the control flow.
 * @param a The asm instruction.
 * @return 1 (true) or 0 (false).
 */
int isControl(const char *a)
{
    if (strlen(a) > 0)
    {
        if (endWith(a, ":"))
        {
            return 1;
        }
        if (startWith(a, "j")
            || startWith(a, "b")
            || startWith(a, "-")
            || startWith(a, "+"))
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Create an array of strings from a file
    without comment and leading/trailing white spaces.
    Accept an ASM file as argument or stdin.
 * @param argc The number of arguments provided.
 * @param argv The arguments provided.
 * @return A structure (dynArray).
 */
dynArray tidyFile(const int argc, char **argv)
{
    char buf[MAXLEN_LINE];
    char **lines = NULL;
    size_t nptrs = 10;
    size_t used  = 0;
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

        if (!startWith(buf, COMMENT))
        {
            if (used == nptrs)
            {
                void *tmp = realloc(lines, (2 * nptrs) * sizeof(*lines));
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
            memcpy(lines[used], trimWhiteSpace(buf), len + 1);
            used += 1;
        }
    }
    if (fp != stdin)
        fclose(fp);

    dynArray r = { lines, used };
    return r;
}

/**
 * @brief Create an array of string to store
    block bss instructions (first word only).
 * @param text The array of strings.
 * @param n The number of elements in the array.
 * @return A structure (dynArray).
 */
dynArray storeBss(dynArray file)
{
    char **text    = file.arr;
    const size_t n = file.used;
    char **bss     = NULL;

    size_t used   = 0;
    size_t bss_on = 0;
    size_t nptrs  = n;
    size_t len;

    if ((bss = malloc(nptrs * (sizeof *bss))) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < n; i++)
    {
        if (matchStr(text[i], BSS_SECTION_START))
        {
            bss_on = 1;
            continue;
        }
        if (matchStr(text[i], SECTION_END) && bss_on)
        {
            bss_on = 0;
            continue;
        }
        if (!matchStr(text[i], BSS_SECTION_START) && bss_on)
        {
            len = strlen(text[i]);

            if ((bss[used] = malloc(len + 1)) == NULL)
            {
                perror("malloc-lines");
                exit(EXIT_FAILURE);
            }
            memcpy(bss[used], text[i], len + 1);
            // Get the first word only.
            strtok(bss[used], " ");
            used += 1;
        }
    }

    dynArray b = { bss, used };
    return b;
}

/**
 * @brief Optimize ASM code.
 * @param file The asm file cleaned
 * @param bss The bss section (only forst words)
 */
void optimizeAsm(dynArray file, dynArray bss, size_t verbose)
{
    char **text           = file.arr;
    const size_t text_len = file.used;

    char **bsswords           = bss.arr;
    const size_t bsswords_len = bss.used;

    size_t totalopt = 0;  // Total number of optimizations performed
    int opted       = -1; // Have we Optimized in this pass
    size_t opass    = 0;  // Optimization pass counter

    dynArray r, r1; // To store regexMatchGroups returns

    char snp_buf1[MAXLEN_LINE],
        snp_buf2[MAXLEN_LINE]; // Store snprintf buffers

    /* Manage pointers */
    char **arr = NULL;
    // size_t nptrs = text_len;

    if ((arr = malloc(text_len * sizeof *arr)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    opass += 1;
    if (verbose)
        fprintf(stderr, "optimization pass %lu: ", opass);

    opted = 0;

    dynArray text_opt = { arr, 0 };

    size_t i = 0;
    while (i < text_len)
    {
        if (startWith(text[i], "st"))
        {
            /* Eliminate redundant stores */
            r = regexMatchGroups(text[i], STORE_AXYZ_TO_PSEUDO, 3);
            if (r.arr != NULL)
            {
                size_t doopt = 0;
                for (size_t j = (i + 1); j < (size_t)min(text_len, (i + 30)); j++)
                {
                    snprintf(snp_buf1, sizeof(snp_buf1), "st([axyz]).b tcc__%s$", r.arr[2]);
                    r1 = regexMatchGroups(text[j], snp_buf1, 2);
                    if (r1.arr != NULL)
                    {
                        printf("[USECASE #1] %lu: %s\n", j, text[j]);

                        freedynArray(r1);

                        doopt = 1;
                        break;
                    }
                    /* Before function call (will be clobbered anyway) */
                    if (startWith(text[j], "jsr.l ")
                        && !startWith(text[j], "jsr.l tcc__"))
                    {
                        printf("[USECASE #2] %lu: %s\n", j, text[j]);

                        doopt = 1;
                        break;
                    }
                    /* Cases in which we don't pursue optimization further
                        #1 Branch or other use of the pseudo register */
                    snprintf(snp_buf1, sizeof(snp_buf1), "tcc__%s", r.arr[2]);
                    if (isControl(text[j]) || isInText(text[j], snp_buf1))
                    {
                        printf("[USECASE #3] %lu: %s\n", j, text[j]);

                        break;
                    }
                    /* #2 Use as a pointer */
                    snprintf(snp_buf1, sizeof(snp_buf1), "[tcc__%s", r.arr[2]);
                    char *ss_buffer = sliceStr(snp_buf1, 0, strlen(snp_buf1) - 1); // Remove the last char
                    if (endWith(r.arr[2], "h")
                        && isInText(text[j], ss_buffer))
                    {
                        printf("[USECASE #4] %lu: %s\n", j, text[j]);

                        free(ss_buffer);

                        break;
                    }
                    free(ss_buffer);
                }
                freedynArray(r);
                if (doopt)
                {
                    i += 1; // Skip redundant store
                    opted += 1;
                    continue;
                }
            }
            /* Stores (x/y) to pseudo-registers */
            r = regexMatchGroups(text[i], STORE_XY_TO_PSEUDO, 3);
            if (r.arr != NULL)
            {
                /* Store hwreg to preg, push preg,
                    function call -> push hwreg, function call */
                snprintf(snp_buf1, sizeof(snp_buf1), "pei (tcc__%s)",
                         r.arr[2]);
                if (matchStr(text[i + 1], snp_buf1)
                    && startWith(text[i + 2], "jsr.l "))
                {
                    printf("[USECASE #5] %lu: %s\n", i + 1, text[i + 1]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "ph%s", r.arr[1]);
                    text_opt = pushToArray(text_opt, snp_buf1);

                    freedynArray(r);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store hwreg to preg, push preg -> store hwreg to preg,
                    push hwreg (shorter) */
                if (matchStr(text[i + 1], snp_buf1))
                {
                    printf("[USECASE #6] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, text[i]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "ph%s", r.arr[1]);
                    text_opt = pushToArray(text_opt, snp_buf1);

                    freedynArray(r);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store hwreg to preg, load hwreg from preg -> store hwreg to
                   preg, transfer hwreg/hwreg (shorter) */
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s",
                         r.arr[2]);
                snprintf(snp_buf2, sizeof(snp_buf2),
                         "lda.b tcc__%s ; DON'T OPTIMIZE", r.arr[2]);
                if (matchStr(text[i + 1], snp_buf1)
                    || matchStr(text[i + 1], snp_buf2))
                {
                    printf("[USECASE #7] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, text[i]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "t%sa",
                             r.arr[1]); // FIXME: shouldn't this be marked as
                                        // DON'T OPTIMIZE again?
                    text_opt = pushToArray(text_opt, snp_buf1);

                    freedynArray(r);

                    i += 2;
                    opted += 1;
                    continue;
                }
                freedynArray(r);
            }
            /* Stores (accu only) to pseudo-registers */
            r = regexMatchGroups(text[i], STORE_A_TO_PSEUDO, 2);
            if (r.arr != NULL)
            {
                /* Store preg followed by load preg */
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s",
                         r.arr[1]);
                if (matchStr(text[i + 1], snp_buf1))
                {
                    printf("[USECASE #8] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, text[i]);

                    freedynArray(r);

                    i += 2; // Omit load
                    opted += 1;
                    continue;
                }
                /* Store preg followed by load preg with ldx/ldy in between */
                if ((startWith(text[i + 1], "ldx")
                     || startWith(text[i + 1], "ldy"))
                    && matchStr(text[i + 2], snp_buf1))
                {
                    printf("[USECASE #9] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, text[i]);
                    text_opt = pushToArray(text_opt, text[i + 1]);

                    freedynArray(r);

                    i += 3; // Omit load
                    opted += 1;
                    continue;
                }
                /* Store accu to preg, push preg, function call -> push accu,
                    function call */
                snprintf(snp_buf1, sizeof(snp_buf1), "pei (tcc__%s)",
                         r.arr[1]);
                if (matchStr(text[i + 1], snp_buf1)
                    && startWith(text[i + 2], "jsr.l "))
                {
                    printf("[USECASE #10] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, "pha");

                    freedynArray(r);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store accu to preg, push preg -> store accu to preg,
                    push accu (shorter) */
                if (matchStr(text[i + 1], snp_buf1))
                {
                    printf("[USECASE #11] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, text[i]);
                    text_opt = pushToArray(text_opt, "pha");

                    freedynArray(r);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store accu to preg1, push preg2, push preg1 -> store accu to
                   preg1, push preg2, push accu */
                else if (startWith(text[i + 1], "pei ")
                         && matchStr(text[i + 2], snp_buf1))
                {
                    printf("[USECASE #12] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, text[i + 1]);
                    text_opt = pushToArray(text_opt, text[i]);
                    text_opt = pushToArray(text_opt, "pha");

                    freedynArray(r);

                    i += 3;
                    opted += 1;
                    continue;
                }
                /* Convert incs/decs on pregs incs/decs on hwregs */
                size_t cont        = 0;
                const char *crem[] = { "inc", "dec" };
                for (size_t k = 0; k < sizeof(crem) / sizeof(const char *); k++)
                {
                    snprintf(snp_buf1, sizeof(snp_buf1), "%s.b tcc__%s",
                             crem[k], r.arr[1]);
                    if (matchStr(text[i + 1], snp_buf1))
                    {
                        printf("[USECASE #13] %lu: %s\n", i + 1, text[i + 1]);
                        /* Store to preg followed by crement on preg */
                        if (matchStr(text[i + 2], snp_buf1)
                            && startWith(text[i + 3], "lda"))
                        {

                            printf("[USECASE #14] %lu: %s\n", i + 2, text[i + 2]);

                            /* Store to preg followed by two crements on preg
                                increment the accu first, then store it to preg
                             */
                            snprintf(snp_buf1, sizeof(snp_buf1), "%s a",
                                     crem[k]);
                            text_opt = pushToArray(text_opt, snp_buf1);
                            text_opt = pushToArray(text_opt, snp_buf1);
                            snprintf(snp_buf1, sizeof(snp_buf1),
                                     "sta.b tcc__%s", r.arr[1]);
                            text_opt = pushToArray(text_opt, snp_buf1);

                            /* A subsequent load can be omitted (the right value
                             * is already in the accu) */
                            snprintf(snp_buf1, sizeof(snp_buf1),
                                     "lda.b tcc__%s", r.arr[1]);
                            if (matchStr(text[i + 3], snp_buf1))
                            {

                                printf("[USECASE #15] %lu: %s\n", i + 3,
                                       text[i + 3]);

                                i += 4;
                            }
                            else
                            {

                                printf("[USECASE #16] %lu: %s\n", i, text[i]);

                                i += 3;
                            }
                            freedynArray(r);

                            opted += 1;
                            cont += 1;
                            break;
                        }
                        else if (startWith(text[i + 2], "lda"))
                        {

                            printf("[USECASE #17] %lu: %s\n", i + 2, text[i + 2]);

                            snprintf(snp_buf1, sizeof(snp_buf1), "%s a",
                                     crem[k]);
                            text_opt = pushToArray(text_opt, snp_buf1);

                            snprintf(snp_buf1, sizeof(snp_buf1),
                                     "sta.b tcc__%s", r.arr[1]);
                            text_opt = pushToArray(text_opt, snp_buf1);

                            snprintf(snp_buf1, sizeof(snp_buf1),
                                     "lda.b tcc__%s", r.arr[1]);
                            if (matchStr(text[i + 2], snp_buf1))
                            {

                                printf("[USECASE #18] %lu: %s\n", i + 2,
                                       text[i + 2]);

                                i += 3;
                            }
                            else
                            {

                                printf("[USECASE #19] %lu: %s\n", i, text[i]);

                                i += 2;
                            }

                            freedynArray(r);

                            opted += 1;
                            cont += 1;
                            break;
                        }
                    }
                }
                if (cont)
                    continue;

                r1 = regexMatchGroups(text[i + 1], "lda.b tcc__([rf][0-9]{0,})",
                                      2);
                if (r1.arr != NULL)
                {
                    printf("[USECASE #20] %lu: %s\n", i + 1, text[i + 1]);

                    char *ss_buffer = sliceStr(text[i + 2], 0, 3);
                    if (matchStr(ss_buffer, "and")
                        || matchStr(ss_buffer, "ora"))
                    {
                        printf("[USECASE #21] %lu: %s\n", i + 2, text[i + 2]);

                        /* Store to preg1, load from preg2, and/or preg1 ->
                         * store to preg1, and/or preg2 */
                        snprintf(snp_buf1, sizeof(snp_buf1), ".b tcc__%s",
                                 r.arr[1]);
                        if (endWith(text[i + 2], snp_buf1))
                        {

                            printf("[USECASE #22] %lu: %s\n", i + 2, text[i + 2]);

                            text_opt = pushToArray(text_opt, text[i]);

                            snprintf(snp_buf1, sizeof(snp_buf1), "%s.b tcc__%s",
                                     ss_buffer, r1.arr[1]);
                            text_opt = pushToArray(text_opt, snp_buf1);

                            free(ss_buffer);
                            freedynArray(r);
                            freedynArray(r1);

                            i += 3;
                            opted += 1;
                            continue;
                        }
                    }
                    free(ss_buffer);
                    freedynArray(r1);
                }

                /* Store to preg, switch to 8 bits, load from preg => skip the
                 * load */
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s",
                         r.arr[1]);
                if (matchStr(text[i + 1], "sep #$20")
                    && matchStr(text[i + 2], snp_buf1))
                {

                    printf("[USECASE #23] %lu: %s\n", i + 2, text[i + 2]);

                    text_opt = pushToArray(text_opt, text[i]);
                    text_opt = pushToArray(text_opt, text[i + 1]);

                    freedynArray(r);

                    i += 3; // Skip load
                    opted += 1;
                    continue;
                }

                /* Two stores to preg without control flow or other uses of preg
                 * => skip first store
                 */
                snprintf(snp_buf1, sizeof(snp_buf1), "tcc__%s", r.arr[1]);
                if (!isControl(text[i + 1]) && !isInText(text[i + 1], snp_buf1))
                {
                    printf("[USECASE #24] %lu: %s\n", i + 1, text[i + 1]);

                    if (matchStr(text[i + 2], text[i]))
                    {

                        printf("[USECASE #25] %lu: %s\n", i + 2, text[i + 2]);

                        text_opt = pushToArray(text_opt, text[i + 1]);
                        text_opt = pushToArray(text_opt, text[i + 2]);

                        freedynArray(r);

                        i += 3; // Skip first store
                        opted += 1;
                        continue;
                    }
                }

                /* Store hwreg to preg, load hwreg from preg -> store hwreg to
                   preg, transfer hwreg/hwreg (shorter) */
                snprintf(snp_buf1, sizeof(snp_buf1), "ld([xy]).b tcc__%s",
                         r.arr[1]);
                r1 = regexMatchGroups(text[i + 1], snp_buf1, 2);
                if (r1.arr != NULL)
                {

                    printf("[USECASE #26] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, text[i]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "ta%s", r1.arr[1]);
                    text_opt = pushToArray(text_opt, snp_buf1);

                    freedynArray(r);
                    freedynArray(r1);

                    i += 2;
                    opted += 1;
                    continue;
                }

                /* Store accu to preg then load accu from preg,
                    with something in-between that does not alter */
                snprintf(snp_buf1, sizeof(snp_buf1), "tcc__%s", r.arr[1]);
                if (!(isControl(text[i + 1]) || changeAccu(text[i + 1])
                      || isInText(text[i + 1], snp_buf1)))
                {

                    printf("[USECASE #27] %lu: %s\n", i + 1, text[i + 1]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s",
                             r.arr[1]);
                    if (matchStr(text[i + 2], snp_buf1))
                    {
                        printf("[USECASE #28] %lu: %s\n", i + 1, text[i + 1]);

                        text_opt = pushToArray(text_opt, text[i]);
                        text_opt = pushToArray(text_opt, text[i + 1]);

                        freedynArray(r);

                        i += 3; // Skip load
                        opted += 1;
                        continue;
                    }
                }

                /* Store preg1, clc, load preg2,
                    add preg1 -> store preg1, clc, add preg2 */
                if (matchStr(text[i + 1], "clc"))
                {

                    printf("[USECASE #29] %lu: %s\n", i + 1, text[i + 1]);

                    r1 = regexMatchGroups(text[i + 2], "lda.b tcc__(r[0-9]{0,})",
                                          2);
                    if (r1.arr != NULL)
                    {
                        snprintf(snp_buf1, sizeof(snp_buf1), "adc.b tcc__%s",
                                 r.arr[1]);
                        if (matchStr(text[i + 3], snp_buf1))
                        {

                            printf("[USECASE #30] %lu: %s\n", i + 3, text[i + 3]);

                            text_opt = pushToArray(text_opt, text[i]);
                            text_opt = pushToArray(text_opt, text[i + 1]);

                            snprintf(snp_buf1, sizeof(snp_buf1),
                                     "adc.b tcc__%s", r1.arr[1]);
                            text_opt = pushToArray(text_opt, snp_buf1);

                            freedynArray(r);
                            freedynArray(r1);

                            i += 4; // Skip load
                            opted += 1;
                            continue;
                        }
                        freedynArray(r1);
                    }
                }

                /* Store accu to preg, asl preg => asl accu, store accu to preg
                    FIXME: is this safe? can we rely on code not making
                   assumptions about the contents of the accu after the shift?
                 */
                snprintf(snp_buf1, sizeof(snp_buf1), "asl.b tcc__%s",
                         r.arr[1]);
                if (matchStr(text[i + 1], snp_buf1))
                {

                    printf("[USECASE #31] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, "asl a");
                    text_opt = pushToArray(text_opt, text[i]);

                    freedynArray(r);

                    i += 2;
                    opted += 1;
                    continue;
                }
                freedynArray(r);
            }

            r = regexMatchGroups(text[i], "sta (.{0,}),s$", 2);
            if (r.arr != NULL)
            {
                snprintf(snp_buf1, sizeof(snp_buf1), "lda %s,s", r.arr[1]);
                if (matchStr(text[i + 1], snp_buf1))
                {

                    printf("[USECASE #32] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, text[i]);

                    freedynArray(r);

                    i += 2; // Omit load
                    opted += 1;
                    continue;
                }
                freedynArray(r);
            }
        } // End of startWith(text[i], "st")

        if (startWith(text[i], "ld"))
        {

            printf("[USECASE #33] %lu: %s\n", i, text[i]);

            r = regexMatchGroups(text[i], "ldx #0", 1);
            if (r.arr != NULL)
            {
                printf("[USECASE #34] %lu: %s\n", i, text[i]);

                r1 = regexMatchGroups(text[i], "lda.l (.{0,}),x$", 2);
                if (r1.arr != NULL && !endWith(text[i + 3], ",x"))
                {

                    printf("[USECASE #35] %lu: %s\n", i + 3, text[i + 3]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "lda.l %s",
                             r1.arr[1]);
                    text_opt = pushToArray(text_opt, snp_buf1);

                    freedynArray(r1);
                    freedynArray(r);

                    i += 2;
                    opted += 1;
                    continue;
                }
                else if (r1.arr != NULL)
                {

                    printf("[USECASE #36] %lu: %s\n", i, text[i]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "lda.l %s", r1.arr[1]);
                    text_opt = pushToArray(text_opt, snp_buf1);

                    text_opt = pushToArray(text_opt, text[i + 2]);

                    char *rs_buffer = replaceStr(text[i + 3], ",x", "");
                    text_opt        = pushToArray(text_opt, rs_buffer);

                    freedynArray(r1);
                    freedynArray(r);

                    i += 4;
                    opted += 1;
                    continue;
                }
                freedynArray(r);
            }

            if (startWith(text[i], "lda.w #")
                && matchStr(text[i + 1], "sta.b tcc__r9")
                && startWith(text[i + 2], "lda.w #")
                && matchStr(text[i + 3], "sta.b tcc__r9h")
                && matchStr(text[i + 4], "sep #$20")
                && startWith(text[i + 5], "lda.b ")
                && matchStr(text[i + 6], "sta.b [tcc__r9]")
                && matchStr(text[i + 7], "rep #$20"))
            {

                printf("[USECASE #37] %lu: %s\n", i, text[i]);

                text_opt = pushToArray(text_opt, "sep #$20");
                text_opt = pushToArray(text_opt, text[i + 5]);

                char *ss_buffer  = sliceStr(text[i + 2], 7, strlen(text[i + 2]));
                char *ss_buffer2 = sliceStr(text[i], 7, strlen(text[i]));
                snprintf(snp_buf1, sizeof(snp_buf1), "sta.l %lu",
                         atol(ss_buffer) * 65536 + atol(ss_buffer2));
                text_opt = pushToArray(text_opt, snp_buf1);

                text_opt = pushToArray(text_opt, "rep #$20");

                free(ss_buffer);
                free(ss_buffer2);

                i += 8;
                opted += 1;
                continue;
            }

            if (matchStr(text[i], "lda.w #0"))
            {
                printf("[USECASE #38] %lu: %s\n", i, text[i]);

                if (startWith(text[i + 1], "sta.b ")
                    && startWith(text[i + 2], "lda"))
                {

                    printf("[USECASE #39] %lu: %s\n", i + 1, text[i + 1]);

                    char *rs_buffer = replaceStr(text[i + 1], "sta.", "stz.");
                    text_opt        = pushToArray(text_opt, rs_buffer);

                    i += 2;
                    opted += 1;
                    continue;
                }
            }
            else if (startWith(text[i], "lda.w #"))
            {

                printf("[USECASE #40] %lu: %s\n", i, text[i]);

                if (matchStr(text[i + 1], "sep #$20")
                    && startWith(text[i + 2], "sta ")
                    && matchStr(text[i + 4], "rep #$20")
                    && startWith(text[i + 4], "lda"))
                {

                    printf("[USECASE #41] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(text_opt, "sep #$20");

                    char *rs_buffer = replaceStr(text[i], "lda.w", "lda.b");
                    text_opt        = pushToArray(text_opt, rs_buffer);

                    text_opt = pushToArray(text_opt, text[i + 2]);
                    text_opt = pushToArray(text_opt, text[i + 3]);

                    i += 4;
                    opted += 1;
                    continue;
                }
            }

            if (startWith(text[i], "lda.b")
                && !isControl(text[i + 1])
                && !isInText(text[i + 1], "a")
                && startWith(text[i + 2], "lda.b"))
            {
                printf("[USECASE #42] %lu: %s\n", i, text[i]);

                text_opt = pushToArray(text_opt, text[i + 1]);
                text_opt = pushToArray(text_opt, text[i + 2]);

                i += 3;
                opted += 1;
                continue;
            }

            /* Don't write preg high back to stack if
                it hasn't been updated */
            if (endWith(text[i + 1], "h")
                && startWith(text[i + 1], "sta.b tcc__r")
                && startWith(text[i], "lda ")
                && endWith(text[i], ",s"))
            {

                printf("[USECASE #43] %lu: %s\n", i + 1, text[i + 1]);

                char *local = sliceStr(text[i], 4, strlen(text[i]));
                char *reg   = sliceStr(text[i + 1], 6, strlen(text[i + 1]));

                /* lda stack ; store high preg ; ...
                    ; load high preg ; sta stack */
                size_t j = i + 2;
                while (j < (text_len - 2)
                       && !isControl(text[j])
                       && !isInText(text[j], reg))
                {
                    printf("[USECASE #44] %lu: %s\n", j, text[j]);
                    j += 1;
                }
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b %s", reg);
                snprintf(snp_buf2, sizeof(snp_buf2), "sta %s", local);
                if (matchStr(text[j], snp_buf1)
                    && matchStr(text[j + 1], snp_buf2))
                {
                    while (i < j)
                    {
                        text_opt = pushToArray(text_opt, text[i]);

                        i += 1;
                    }

                    free(reg);
                    free(local);

                    i += 2; // Skip load high preg ; sta stack
                    opted += 1;
                    continue;
                }
                free(reg);
                free(local);
            }

            /* Reorder copying of 32-bit value to preg if it looks as
                if that could allow further optimization.
                Looking for:
                    lda something
                    sta.b tcc_rX
                    lda something
                    sta.b tcc_rYh
                    ...tcc_rX...
            */
            if (startWith(text[i], "lda")
                && startWith(text[i + 1], "sta.b tcc__r"))
            {
                printf("[USECASE #45] %lu: %s\n", i, text[i]);

                char *reg = sliceStr(text[i + 1], 6, strlen(text[i + 1]));
                if (!endWith(reg, "h")
                    && startWith(text[i + 2], "lda")
                    && !endWith(text[i + 2], reg)
                    && startWith(text[i + 3], "sta.b tcc__r")
                    && endWith(text[i + 3], "h")
                    && endWith(text[i + 4], reg))
                {
                    printf("[USECASE #46] %lu: %s\n", i + 2, text[i + 2]);

                    text_opt = pushToArray(text_opt, text[i + 2]);
                    text_opt = pushToArray(text_opt, text[i + 3]);
                    text_opt = pushToArray(text_opt, text[i]);
                    text_opt = pushToArray(text_opt, text[i + 1]);

                    free(reg);

                    i += 4;
                    // this is not an optimization per se, so we don't count it
                    continue;
                }
                free(reg);
            }

            /* Compare optimizations inspired by optimore
                These opts simplify compare operations, which are monstrous because
                they have to take the long long case into account.
                We try to detect those cases by checking if a tya follows the
                comparison (not sure if this is reliable, but it passes the test suite)
            */
            if (matchStr(text[i], "ldx #1")
                && startWith(text[i + 1], "lda.b tcc__")
                && matchStr(text[i + 2], "sec")
                && startWith(text[i + 3], "sbc #")
                && matchStr(text[i + 4], "tay")
                && matchStr(text[i + 5], "beq +")
                && matchStr(text[i + 6], "dex")
                && matchStr(text[i + 7], "+")
                && startWith(text[i + 8], "stx.b tcc__")
                && matchStr(text[i + 9], "txa")
                && matchStr(text[i + 10], "bne +")
                && startWith(text[i + 11], "brl ")
                && matchStr(text[i + 12], "+")
                && !matchStr(text[i + 13], "tya"))
            {
                printf("[USECASE #47] %lu: %s\n", i, text[i]);

                text_opt = pushToArray(text_opt, text[i + 1]);

                char *ins = sliceStr(text[i + 3], 5, strlen(text[i + 1]));
                snprintf(snp_buf1, sizeof(snp_buf1), "cmp #%s", ins);
                text_opt = pushToArray(text_opt, snp_buf1);

                text_opt = pushToArray(text_opt, text[i + 5]);
                text_opt = pushToArray(text_opt, text[i + 11]); // brl
                text_opt = pushToArray(text_opt, text[i + 12]); // +

                free(ins);

                i += 13;
                opted += 1;
                continue;
            }

            if (matchStr(text[i], "ldx #1")
                && matchStr(text[i + 1], "sec")
                && startWith(text[i + 2], "sbc #")
                && matchStr(text[i + 3], "tay")
                && matchStr(text[i + 4], "beq +")
                && matchStr(text[i + 5], "dex")
                && matchStr(text[i + 6], "+")
                && startWith(text[i + 7], "stx.b tcc__")
                && matchStr(text[i + 8], "txa")
                && matchStr(text[i + 9], "bne +")
                && startWith(text[i + 10], "brl ")
                && matchStr(text[i + 11], "+")
                && !matchStr(text[i + 12], "tya"))
            {
                printf("[USECASE #48] %lu: %s\n", i, text[i]);

                char *ins = sliceStr(text[i + 2], 5, strlen(text[i + 2]));
                snprintf(snp_buf1, sizeof(snp_buf1), "cmp #%s", ins);
                text_opt = pushToArray(text_opt, snp_buf1);

                text_opt = pushToArray(text_opt, text[i + 4]);
                text_opt = pushToArray(text_opt, text[i + 10]); // brl
                text_opt = pushToArray(text_opt, text[i + 11]); // +

                free(ins);

                i += 12;
                opted += 1;
                continue;
            }

            if (matchStr(text[i], "ldx #1")
                && startWith(text[i + 1], "lda.b tcc__r")
                && matchStr(text[i + 2], "sec")
                && startWith(text[i + 3], "sbc.b tcc__r")
                && matchStr(text[i + 4], "tay")
                && matchStr(text[i + 5], "beq +")
                && matchStr(text[i + 6], "bcs ++")
                && matchStr(text[i + 7], "+ dex")
                && matchStr(text[i + 8], "++")
                && startWith(text[i + 9], "stx.b tcc__r")
                && matchStr(text[i + 10], "txa")
                && matchStr(text[i + 11], "bne +")
                && startWith(text[i + 12], "brl ")
                && matchStr(text[i + 13], "+")
                && !matchStr(text[i + 14], "tya"))
            {
                printf("[USECASE #49] %lu: %s\n", i, text[i]);

                text_opt = pushToArray(text_opt, text[i + 1]);

                char *ins = sliceStr(text[i + 3], 6, strlen(text[i + 3]));
                snprintf(snp_buf1, sizeof(snp_buf1), "cmp.b %s", ins);
                text_opt = pushToArray(text_opt, snp_buf1);

                text_opt = pushToArray(text_opt, text[i + 5]);
                text_opt = pushToArray(text_opt, "bcc +");
                text_opt = pushToArray(text_opt, "brl ++");
                text_opt = pushToArray(text_opt, "+");
                text_opt = pushToArray(text_opt, text[i + 12]);
                text_opt = pushToArray(text_opt, "++");

                free(ins);

                i += 14;
                opted += 1;
                continue;
            }

            if (matchStr(text[i], "ldx #1")
                && matchStr(text[i + 1], "sec")
                && startWith(text[i + 2], "sbc.w #")
                && matchStr(text[i + 3], "tay")
                && matchStr(text[i + 4], "bvc +")
                && matchStr(text[i + 5], "eor #$8000")
                && matchStr(text[i + 6], "+")
                && matchStr(text[i + 7], "bmi +++")
                && matchStr(text[i + 8], "++")
                && matchStr(text[i + 9], "dex")
                && matchStr(text[i + 10], "+++")
                && startWith(text[i + 11], "stx.b tcc__r")
                && matchStr(text[i + 12], "txa")
                && matchStr(text[i + 13], "bne +")
                && startWith(text[i + 14], "brl ")
                && matchStr(text[i + 15], "+")
                && !matchStr(text[i + 16], "tya"))
            {
                printf("[USECASE #50] %lu: %s\n", i, text[i]);

                text_opt = pushToArray(text_opt, text[i + 1]);
                text_opt = pushToArray(text_opt, text[i + 2]);
                text_opt = pushToArray(text_opt, text[i + 4]);
                text_opt = pushToArray(text_opt, "eor #$8000");
                text_opt = pushToArray(text_opt, "+");
                text_opt = pushToArray(text_opt, "bmi +");
                text_opt = pushToArray(text_opt, text[i + 14]);
                text_opt = pushToArray(text_opt, "+");

                i += 16;
                opted += 1;
                continue;
            }

            if (matchStr(text[i], "ldx #1")
                && startWith(text[i + 1], "lda.b tcc__r")
                && matchStr(text[i + 2], "sec")
                && startWith(text[i + 3], "sbc.b tcc__r")
                && matchStr(text[i + 4], "tay")
                && matchStr(text[i + 5], "bvc +")
                && matchStr(text[i + 6], "eor #$8000")
                && matchStr(text[i + 7], "+")
                && matchStr(text[i + 8], "bmi +++")
                && matchStr(text[i + 9], "++")
                && matchStr(text[i + 10], "dex")
                && matchStr(text[i + 11], "+++")
                && startWith(text[i + 12], "stx.b tcc__r")
                && matchStr(text[i + 13], "txa")
                && matchStr(text[i + 14], "bne +")
                && startWith(text[i + 15], "brl ")
                && matchStr(text[i + 16], "+")
                && !matchStr(text[i + 17], "tya"))
            {
                printf("[USECASE #51] %lu: %s\n", i, text[i]);

                text_opt = pushToArray(text_opt, text[i + 1]);
                text_opt = pushToArray(text_opt, text[i + 2]);
                text_opt = pushToArray(text_opt, text[i + 3]);
                text_opt = pushToArray(text_opt, text[i + 5]);
                text_opt = pushToArray(text_opt, text[i + 6]);
                text_opt = pushToArray(text_opt, "+");
                text_opt = pushToArray(text_opt, "bmi +");
                text_opt = pushToArray(text_opt, text[i + 15]);
                text_opt = pushToArray(text_opt, "+");

                i += 17;
                opted += 1;
                continue;
            }

            if (matchStr(text[i], "ldx #1")
                && matchStr(text[i + 1], "sec")
                && startWith(text[i + 2], "sbc.b tcc__r")
                && matchStr(text[i + 3], "tay")
                && matchStr(text[i + 4], "bvc +")
                && matchStr(text[i + 5], "eor #$8000")
                && matchStr(text[i + 6], "+")
                && matchStr(text[i + 7], "bmi +++")
                && matchStr(text[i + 8], "++")
                && matchStr(text[i + 9], "dex")
                && matchStr(text[i + 10], "+++")
                && startWith(text[i + 11], "stx.b tcc__r")
                && matchStr(text[i + 12], "txa")
                && matchStr(text[i + 13], "bne +")
                && startWith(text[i + 14], "brl ")
                && matchStr(text[i + 15], "+")
                && !matchStr(text[i + 16], "tya"))
            {
                printf("[USECASE #52] %lu: %s\n", i, text[i]);

                text_opt = pushToArray(text_opt, text[i + 1]);
                text_opt = pushToArray(text_opt, text[i + 2]);
                text_opt = pushToArray(text_opt, text[i + 4]);
                text_opt = pushToArray(text_opt, text[i + 5]);
                text_opt = pushToArray(text_opt, "+");
                text_opt = pushToArray(text_opt, "bmi +");
                text_opt = pushToArray(text_opt, text[i + 14]);
                text_opt = pushToArray(text_opt, "+");

                i += 16;
                opted += 1;
                continue;
            }
        } // End of startWith(text[i], "ld")

        if (matchStr(text[i], "rep #$20")
            && matchStr(text[i + 1], "sep #$20"))
        {
            printf("[USECASE #53] %lu: %s\n", i, text[i]);

            i += 2;
            opted += 1;
            continue;
        }

        if (matchStr(text[i], "sep #$20")
            && startWith(text[i + 1], "lda #")
            && matchStr(text[i + 2], "pha")
            && startWith(text[i + 3], "lda #")
            && matchStr(text[i + 4], "pha"))
        {
            printf("[USECASE #54] %lu: %s\n", i, text[i]);
            char *token1, *token2;
            token1 = splitStr(text[i + 1], "#", 1);
            token2 = splitStr(text[i + 3], "#", 1);
            snprintf(snp_buf1, sizeof(snp_buf1), "pea.w (%s * 256 + %s)", token1, token2);
            text_opt = pushToArray(text_opt, snp_buf1);
            text_opt = pushToArray(text_opt, text[i]);

            i += 5;
            opted += 1;
            continue;
        }

        r = regexMatchGroups(text[i], "adc #(.{0,})$", 2);
        if (r.arr != NULL)
        {
            printf("[USECASE #55] %lu: %s\n", i, text[i]);

            r1 = regexMatchGroups(text[i + 1], "sta.b (tcc__[fr][0-9]{0,})$", 2);
            if (r1.arr != NULL)
            {
                printf("[USECASE #56] %lu: %s\n", i + 1, text[i + 1]);

                snprintf(snp_buf1, sizeof(snp_buf1), "inc.b %s",
                         r1.arr[1]);
                if (matchStr(text[i + 2], snp_buf1)
                    && matchStr(text[i + 3], snp_buf1))
                {
                    printf("[USECASE #57] %lu: %s\n", i + 2, text[i + 2]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "adc #%s + 2",
                             r.arr[1]);
                    text_opt = pushToArray(text_opt, snp_buf1);
                    text_opt = pushToArray(text_opt, text[i + 1]);

                    freedynArray(r1);
                    freedynArray(r);

                    i += 4;
                    opted += 1;
                    continue;
                }

                freedynArray(r1);
            }

            freedynArray(r);
        }

        if (strlen(text[i]) >= 6)
        {
            char *ss_buffer = sliceStr(text[i], 0, 6);

            if (matchStr(ss_buffer, "lda.l ")
                || matchStr(ss_buffer, "sta.l "))
            {
                size_t cont      = 0;
                char *ss_buffer2 = sliceStr(text[i], 2, strlen(text[i]));

                for (size_t b = 0; b < bsswords_len; b++)
                {
                    snprintf(snp_buf1, sizeof(snp_buf1), "a.l %s ",
                             bsswords[b]);
                    if (startWith(ss_buffer2, snp_buf1))
                    {
                        printf("[USECASE #58] %lu: %s\n", i, text[i]);

                        char *rs_buffer = replaceStr(text[i], "a.l", "a.w");
                        text_opt        = pushToArray(text_opt, rs_buffer);

                        i += 1;
                        opted += 1;
                        cont = 1;
                        break;
                    }
                }
                free(ss_buffer2);
                if (cont)
                {
                    free(ss_buffer);
                    continue;
                }
            }
            free(ss_buffer);
        }

        if (startWith(text[i], "jmp.w ")
            || startWith(text[i], "bra __"))
        {
            size_t j    = i + 1;
            size_t cont = 0;
            while (j < text_len && endWith(text[j], ":"))
            {
                char *ss_buffer = sliceStr(text[j], 0, strlen(text[j]) - 1);
                if (endWith(text[i], ss_buffer))
                {
                    printf("[USECASE #59] %lu: %s\n", i, text[i]);

                    free(ss_buffer);
                    i += 1; // Redundant branch, discard it.
                    opted += 1;
                    cont = 1;
                    break;
                }
                j += 1;

                free(ss_buffer);
                if (cont)
                    continue;
            }
        }

        if (startWith(text[i], "jmp.w "))
        {
            printf("[USECASE #60] %lu: %s\n", i, text[i]);

            /* Worst case is a 4-byte instruction, so if the jump target is closer
                than 32 instructions, we can safely substitute a branch */
            char *label = sliceStr(text[i], 6, strlen(text[i]));
            snprintf(snp_buf1, sizeof(snp_buf1), "%s:", label);
            size_t cont = 0;
            for (size_t l = max(0, (i - 32)); l < (size_t)min(text_len, (i + 32)); l++)
            {
                if (matchStr(text[l], snp_buf1))
                {
                    printf("[USECASE #61] %lu: %s\n", l, text[l]);

                    char *rs_buffer = replaceStr(text[i], "jmp.w", "bra");
                    text_opt        = pushToArray(text_opt, rs_buffer);

                    i += 1;
                    opted += 1;
                    cont = 1;
                    break;
                }
            }
            free(label);
            if (cont)
            {
                continue;
            }
        }

        text_opt = pushToArray(text_opt, text[i]);
        i++;

    } // End of while (i < text_len)

    if (verbose)
        fprintf(stderr, "%u optimizations performed\n", opted);

    totalopt += opted;

    printf("\n\n______________[ASM CODE]_________________\n");
    for (size_t i = 0; i < text_opt.used; i++)
    {
        printf("%s\n", text_opt.arr[i]);
    }
    freedynArray(text_opt);

    if (verbose)
        fprintf(stderr, "%lu optimizations performed in total\n", totalopt);
}
