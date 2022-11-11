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
    /* Unmanaged case, should we exit? */
    return 3;
}

/**
 * @brief Checksif it touches the accumulator register.
 * @param a The asm instruction.
 * @return 1 (true) or 0 (false).
 */
int changeAccu(const char *a)
{
    if (strlen(a) > 2)
    {
        if (a[2] == 'a' && (!startWith(a, "pha") && !startWith(a, "sta")))
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
        if (startWith(a, "j") || startWith(a, "b") || startWith(a, "-")
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
dynArray storeBss(char **text, const size_t n)
{
    char **bss = NULL;
    char *saveptr;

    size_t used   = 0;
    size_t bss_on = 0;
    size_t nptrs  = n;
    size_t len;

    if ((bss = malloc(nptrs * sizeof *bss)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < n; i++)
    {
        if (matchString(text[i], BSS_START))
        {
            bss_on = 1;
            continue;
        }
        if (matchString(text[i], BSS_END) && bss_on)
        {
            bss_on = 0;
            continue;
        }
        if (!matchString(text[i], BSS_START) && bss_on)
        {
            len = strlen(text[i]);

            if ((bss[used] = malloc(len + 1)) == NULL)
            {
                perror("malloc-lines");
                exit(EXIT_FAILURE);
            }
            memcpy(bss[used], text[i], len + 1);
            strtok_r(bss[used], " ", &saveptr);
            used += 1;
        }
    }

    dynArray b = { bss, used };
    return b;
}

/**
 * @brief Optimize ASM code.
 * @param text
 * @param n
 */
void optimizeAsm(char **text, const size_t n)
{
    // int totalopt = 0; // Total number of optimizations performed
    int opted = -1; // Have we Optimized in this pass
    // int opass = 0;    // Optimization pass counter

    regexdynArray r, r1;

    /* Store snprintf buffers */
    char snp_buf1[MAXLEN_LINE],
        snp_buf2[MAXLEN_LINE];

    /* Manage pointers */
    char **arr;
    size_t nptrs      = n;
    dynArray text_opt = { NULL, 0 };

    if ((arr = malloc(nptrs * sizeof *arr)) == NULL)
    {
        perror("malloc-lines");
        exit(EXIT_FAILURE);
    }

    size_t i = 0;
    while (i < n)
    {
        if (startWith(text[i], "st"))
        {
            /* Eliminate redundant stores */
            r = regexMatchGroups(text[i], STORE_AXYZ_TO_PSEUDO, 3);
            if (r.status)
            {
                size_t doopt = 0;
                for (size_t j = (i + 1); j < (size_t)findMin(n, (i + 30)); j++)
                {
                    snprintf(snp_buf1, sizeof(snp_buf1),
                             "st([axyz]).b tcc__%s$", r.groups[2]);
                    r1 = regexMatchGroups(text[j], snp_buf1, 2);
                    if (r1.status == 1)
                    {
                        printf("[USECASE #1] %lu: %s\n", j, text[j]);

                        freedynArray(r1.groups, r1.used);

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
                    snprintf(snp_buf1, sizeof(snp_buf1), "tcc__%s",
                             r.groups[2]);
                    if (isControl(text[j]) || isInText(text[j], snp_buf1))
                    {
                        printf("[USECASE #3] %lu: %s\n", j, text[j]);

                        break;
                    }
                    /* #2 Use as a pointer */
                    snprintf(snp_buf1, sizeof(snp_buf1), "[tcc__%s",
                             r.groups[2]);
                    snp_buf1[strlen(snp_buf1) - 1] = '\0'; // Remove the last char
                    if (endWith(r.groups[2], "h")
                        && isInText(text[j], snp_buf1))
                    {
                        printf("[USECASE #4] %lu: %s\n", j, text[j]);

                        break;
                    }
                }
                freedynArray(r.groups, r.used);
                if (doopt)
                {
                    i += 1; // Skip redundant store
                    opted += 1;
                    continue;
                }
            }
            /* Stores (x/y) to pseudo-registers */
            r = regexMatchGroups(text[i], STORE_XY_TO_PSEUDO, 3);
            if (r.status)
            {
                /* Store hwreg to preg, push preg,
                    function call -> push hwreg, function call */
                snprintf(snp_buf1, sizeof(snp_buf1), "pei (tcc__%s)",
                         r.groups[2]);
                if (matchString(text[i + 1], snp_buf1)
                    && startWith(text[i + 2], "jsr.l "))
                {
                    printf("[USECASE #5] %lu: %s\n", i + 1, text[i + 1]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "ph%s", r.groups[1]);
                    text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store hwreg to preg, push preg -> store hwreg to preg,
                    push hwreg (shorter) */
                if (matchString(text[i + 1], snp_buf1))
                {
                    printf("[USECASE #6] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, text[i], text_opt.used);

                    snprintf(snp_buf1, sizeof(snp_buf1), "ph%s", r.groups[1]);
                    text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store hwreg to preg, load hwreg from preg -> store hwreg to
                   preg, transfer hwreg/hwreg (shorter) */
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s",
                         r.groups[2]);
                snprintf(snp_buf2, sizeof(snp_buf2),
                         "lda.b tcc__%s ; DON'T OPTIMIZE", r.groups[2]);
                if (matchString(text[i + 1], snp_buf1)
                    || matchString(text[i + 1], snp_buf2))
                {
                    printf("[USECASE #7] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, text[i], text_opt.used);

                    snprintf(snp_buf1, sizeof(snp_buf1), "t%sa",
                             r.groups[1]); // FIXME: shouldn't this be marked as
                                           // DON'T OPTIMIZE again?
                    text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                freedynArray(r.groups, r.used);
            }
            /* Stores (accu only) to pseudo-registers */
            r = regexMatchGroups(text[i], STORE_A_TO_PSEUDO, 2);
            if (r.status)
            {
                /* Store preg followed by load preg */
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s",
                         r.groups[1]);
                if (matchString(text[i + 1], snp_buf1))
                {
                    printf("[USECASE #8] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, text[i], text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 2; // Omit load
                    opted += 1;
                    continue;
                }
                /* Store preg followed by load preg with ldx/ldy in between */
                if ((startWith(text[i + 1], "ldx")
                     || startWith(text[i + 1], "ldy"))
                    && matchString(text[i + 2], snp_buf1))
                {
                    printf("[USECASE #9] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, text[i], text_opt.used);
                    text_opt = pushToArray(arr, text[i + 1], text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 3; // Omit load
                    opted += 1;
                    continue;
                }
                /* Store accu to preg, push preg, function call -> push accu,
                    function call */
                snprintf(snp_buf1, sizeof(snp_buf1), "pei (tcc__%s)",
                         r.groups[1]);
                if (matchString(text[i + 1], snp_buf1)
                    && startWith(text[i + 2], "jsr.l "))
                {
                    printf("[USECASE #10] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, "pha", text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store accu to preg, push preg -> store accu to preg,
                    push accu (shorter) */
                if (matchString(text[i + 1], snp_buf1))
                {
                    printf("[USECASE #11] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, text[i], text_opt.used);
                    text_opt = pushToArray(arr, "pha", text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                /* Store accu to preg1, push preg2, push preg1 -> store accu to
                   preg1, push preg2, push accu */
                else if (startWith(text[i + 1], "pei ")
                         && matchString(text[i + 2], snp_buf1))
                {
                    printf("[USECASE #12] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, text[i + 1], text_opt.used);
                    text_opt = pushToArray(arr, text[i], text_opt.used);
                    text_opt = pushToArray(arr, "pha", text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 3;
                    opted += 1;
                    continue;
                }
                /* Convert incs/decs on pregs incs/decs on hwregs */
                size_t cont     = 0;
                char crem[2][4] = { "inc", "dec" };
                for (size_t k = 0; k < sizeof(crem) / sizeof(crem[0]); k++)
                {
                    snprintf(snp_buf1, sizeof(snp_buf1), "%s.b tcc__%s",
                             crem[k], r.groups[1]);
                    if (matchString(text[i + 1], snp_buf1))
                    {
                        printf("[USECASE #13] %lu: %s\n", i + 1, text[i + 1]);
                        /* Store to preg followed by crement on preg */
                        if (matchString(text[i + 2], snp_buf1)
                            && startWith(text[i + 3], "lda"))
                        {

                            printf("[USECASE #14] %lu: %s\n", i + 2, text[i + 2]);

                            /* Store to preg followed by two crements on preg
                                increment the accu first, then store it to preg
                             */
                            snprintf(snp_buf1, sizeof(snp_buf1), "%s a",
                                     crem[k]);
                            text_opt = pushToArray(arr, snp_buf1, text_opt.used);
                            text_opt = pushToArray(arr, snp_buf1, text_opt.used);
                            snprintf(snp_buf1, sizeof(snp_buf1),
                                     "sta.b tcc__%s", r.groups[1]);
                            text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                            /* A subsequent load can be omitted (the right value
                             * is already in the accu) */
                            snprintf(snp_buf1, sizeof(snp_buf1),
                                     "lda.b tcc__%s", r.groups[1]);
                            if (matchString(text[i + 3], snp_buf1))
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
                            freedynArray(r.groups, r.used);

                            opted += 1;
                            cont += 1;
                            break;
                        }
                        else if (startWith(text[i + 2], "lda"))
                        {

                            printf("[USECASE #17] %lu: %s\n", i + 2, text[i + 2]);

                            snprintf(snp_buf1, sizeof(snp_buf1), "%s a",
                                     crem[k]);
                            text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                            snprintf(snp_buf1, sizeof(snp_buf1),
                                     "sta.b tcc__%s", r.groups[1]);
                            text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                            snprintf(snp_buf1, sizeof(snp_buf1),
                                     "lda.b tcc__%s", r.groups[1]);
                            if (matchString(text[i + 2], snp_buf1))
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

                            freedynArray(r.groups, r.used);

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
                if (r1.status)
                {
                    printf("[USECASE #20] %lu: %s\n", i + 1, text[i + 1]);

                    char *ss_buffer = sliceStr(text[i + 2], 0, 3);
                    if (matchString(ss_buffer, "and")
                        || matchString(ss_buffer, "ora"))
                    {
                        printf("[USECASE #21] %lu: %s\n", i + 2, text[i + 2]);

                        /* Store to preg1, load from preg2, and/or preg1 ->
                         * store to preg1, and/or preg2 */
                        snprintf(snp_buf1, sizeof(snp_buf1), ".b tcc__%s",
                                 r.groups[1]);
                        if (endWith(text[i + 2], snp_buf1))
                        {

                            printf("[USECASE #22] %lu: %s\n", i + 2, text[i + 2]);

                            text_opt = pushToArray(arr, text[i], text_opt.used);

                            snprintf(snp_buf1, sizeof(snp_buf1), "%s.b tcc__%s",
                                     ss_buffer, r1.groups[1]);
                            text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                            free(ss_buffer);
                            freedynArray(r.groups, r.used);
                            freedynArray(r1.groups, r1.used);

                            i += 3;
                            opted += 1;
                            continue;
                        }
                    }
                    free(ss_buffer);
                    freedynArray(r1.groups, r1.used);
                }

                /* Store to preg, switch to 8 bits, load from preg => skip the
                 * load */
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s",
                         r.groups[1]);
                if (matchString(text[i + 1], "sep #$20")
                    && matchString(text[i + 2], snp_buf1))
                {

                    printf("[USECASE #23] %lu: %s\n", i + 2, text[i + 2]);

                    text_opt = pushToArray(arr, text[i], text_opt.used);
                    text_opt = pushToArray(arr, text[i + 1], text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 3; // Skip load
                    opted += 1;
                    continue;
                }

                /* Two stores to preg without control flow or other uses of preg
                 * => skip first store
                 */
                snprintf(snp_buf1, sizeof(snp_buf1), "tcc__%s", r.groups[1]);
                if (!isControl(text[i + 1]) && !isInText(text[i + 1], snp_buf1))
                {
                    printf("[USECASE #24] %lu: %s\n", i + 1, text[i + 1]);

                    if (matchString(text[i + 2], text[i]))
                    {

                        printf("[USECASE #25] %lu: %s\n", i + 2, text[i + 2]);

                        text_opt = pushToArray(arr, text[i + 1], text_opt.used);
                        text_opt = pushToArray(arr, text[i + 2], text_opt.used);

                        freedynArray(r.groups, r.used);

                        i += 3; // Skip first store
                        opted += 1;
                        continue;
                    }
                }

                /* Store hwreg to preg, load hwreg from preg -> store hwreg to
                   preg, transfer hwreg/hwreg (shorter) */
                snprintf(snp_buf1, sizeof(snp_buf1), "ld([xy]).b tcc__%s",
                         r.groups[1]);
                r1 = regexMatchGroups(text[i + 1], snp_buf1, 2);
                if (r1.status)
                {

                    printf("[USECASE #26] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, text[i], text_opt.used);

                    snprintf(snp_buf1, sizeof(snp_buf1), "ta%s", r1.groups[1]);
                    text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                    freedynArray(r.groups, r.used);
                    freedynArray(r1.groups, r1.used);

                    i += 2;
                    opted += 1;
                    continue;
                }

                /* Store accu to preg then load accu from preg,
                    with something in-between that does not alter */
                snprintf(snp_buf1, sizeof(snp_buf1), "tcc__%s", r.groups[1]);
                if (!(isControl(text[i + 1]) || changeAccu(text[i + 1])
                      || isInText(text[i + 1], snp_buf1)))
                {

                    printf("[USECASE #27] %lu: %s\n", i + 1, text[i + 1]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "lda.b tcc__%s",
                             r.groups[1]);
                    if (matchString(text[i + 2], snp_buf1))
                    {
                        printf("[USECASE #28] %lu: %s\n", i + 1, text[i + 1]);

                        text_opt = pushToArray(arr, text[i], text_opt.used);
                        text_opt = pushToArray(arr, text[i + 1], text_opt.used);

                        freedynArray(r.groups, r.used);

                        i += 3; // Skip load
                        opted += 1;
                        continue;
                    }
                }

                /* Store preg1, clc, load preg2,
                    add preg1 -> store preg1, clc, add preg2 */
                if (matchString(text[i + 1], "clc"))
                {

                    printf("[USECASE #29] %lu: %s\n", i + 1, text[i + 1]);

                    r1 = regexMatchGroups(text[i + 2], "lda.b tcc__(r[0-9]{0,})",
                                          2);
                    if (r1.status)
                    {
                        snprintf(snp_buf1, sizeof(snp_buf1), "adc.b tcc__%s",
                                 r.groups[1]);
                        if (matchString(text[i + 3], snp_buf1))
                        {

                            printf("[USECASE #30] %lu: %s\n", i + 3, text[i + 3]);

                            text_opt = pushToArray(arr, text[i], text_opt.used);
                            text_opt = pushToArray(arr, text[i + 1], text_opt.used);

                            snprintf(snp_buf1, sizeof(snp_buf1),
                                     "adc.b tcc__%s", r1.groups[1]);
                            text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                            freedynArray(r.groups, r.used);
                            freedynArray(r1.groups, r1.used);

                            i += 4; // Skip load
                            opted += 1;
                            continue;
                        }
                        freedynArray(r1.groups, r1.used);
                    }
                }

                /* Store accu to preg, asl preg => asl accu, store accu to preg
                    FIXME: is this safe? can we rely on code not making
                   assumptions about the contents of the accu after the shift?
                 */
                snprintf(snp_buf1, sizeof(snp_buf1), "asl.b tcc__%s",
                         r.groups[1]);
                if (matchString(text[i + 1], snp_buf1))
                {

                    printf("[USECASE #31] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, "asl a", text_opt.used);
                    text_opt = pushToArray(arr, text[i], text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                freedynArray(r.groups, r.used);
            }

            r = regexMatchGroups(text[i], "sta (.{0,}),s$", 2);
            if (r.status)
            {
                snprintf(snp_buf1, sizeof(snp_buf1), "lda %s,s", r.groups[1]);
                if (matchString(text[i + 1], snp_buf1))
                {

                    printf("[USECASE #32] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, text[i], text_opt.used);

                    freedynArray(r.groups, r.used);

                    i += 2; // Omit load
                    opted += 1;
                    continue;
                }
                freedynArray(r.groups, r.used);
            }
        } // End of startWith(text[i], "st")

        if (startWith(text[i], "ld"))
        {

            printf("[USECASE #33] %lu: %s\n", i, text[i]);

            r = regexMatchGroups(text[i], "ldx #0", 1);
            if (r.status)
            {
                printf("[USECASE #34] %lu: %s\n", i, text[i]);

                r1 = regexMatchGroups(text[i], "lda.l (.{0,}),x$", 2);
                if (r1.status && !endWith(text[i + 3], ",x"))
                {

                    printf("[USECASE #35] %lu: %s\n", i + 3, text[i + 3]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "lda.l %s",
                             r1.groups[1]);
                    text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                    freedynArray(r1.groups, r1.used);
                    freedynArray(r.groups, r.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
                else if (r1.status)
                {

                    printf("[USECASE #36] %lu: %s\n", i, text[i]);

                    snprintf(snp_buf1, sizeof(snp_buf1), "lda.l %s", r1.groups[1]);
                    text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                    text_opt = pushToArray(arr, text[i + 2], text_opt.used);

                    char *rs_buffer = replaceStr(text[i + 3], ",x", "");
                    text_opt        = pushToArray(arr, rs_buffer, text_opt.used);

                    freedynArray(r1.groups, r1.used);
                    freedynArray(r.groups, r.used);

                    i += 4;
                    opted += 1;
                    continue;
                }
                freedynArray(r.groups, r.used);
            }

            if (startWith(text[i], "lda.w #")
                && matchString(text[i + 1], "sta.b tcc__r9")
                && startWith(text[i + 2], "lda.w #")
                && matchString(text[i + 3], "sta.b tcc__r9h")
                && matchString(text[i + 4], "sep #$20")
                && startWith(text[i + 5], "lda.b ")
                && matchString(text[i + 6], "sta.b [tcc__r9]")
                && matchString(text[i + 7], "rep #$20"))
            {

                printf("[USECASE #37] %lu: %s\n", i, text[i]);

                text_opt = pushToArray(arr, "sep #$20", text_opt.used);
                text_opt = pushToArray(arr, text[i + 5], text_opt.used);

                char *ss_buffer  = sliceStr(text[i + 2], 7, strlen(text[i + 2]));
                char *ss_buffer2 = sliceStr(text[i], 7, strlen(text[i]));
                snprintf(snp_buf1, sizeof(snp_buf1), "sta.l %lu",
                         atol(ss_buffer) * 65536 + atol(ss_buffer2));
                text_opt = pushToArray(arr, snp_buf1, text_opt.used);

                text_opt = pushToArray(arr, "rep #$20", text_opt.used);

                free(ss_buffer);
                free(ss_buffer2);

                i += 8;
                opted += 1;
                continue;
            }

            if (matchString(text[i], "lda.w #0"))
            {
                printf("[USECASE #38] %lu: %s\n", i, text[i]);

                if (startWith(text[i + 1], "sta.b ")
                    && startWith(text[i + 2], "lda"))
                {

                    printf("[USECASE #39] %lu: %s\n", i + 1, text[i + 1]);

                    char *rs_buffer = replaceStr(text[i + 1], "sta.", "stz.");
                    text_opt        = pushToArray(arr, rs_buffer, text_opt.used);

                    i += 2;
                    opted += 1;
                    continue;
                }
            }
            else if (startWith(text[i], "lda.w #"))
            {

                printf("[USECASE #40] %lu: %s\n", i, text[i]);

                if (matchString(text[i + 1], "sep #$20")
                    && startWith(text[i + 2], "sta ")
                    && matchString(text[i + 4], "rep #$20")
                    && startWith(text[i + 4], "lda"))
                {

                    printf("[USECASE #41] %lu: %s\n", i + 1, text[i + 1]);

                    text_opt = pushToArray(arr, "sep #$20", text_opt.used);

                    char *rs_buffer = replaceStr(text[i], "lda.w", "lda.b");
                    text_opt        = pushToArray(arr, rs_buffer, text_opt.used);

                    text_opt = pushToArray(arr, text[i + 2], text_opt.used);
                    text_opt = pushToArray(arr, text[i + 3], text_opt.used);

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

                text_opt = pushToArray(arr, text[i + 1], text_opt.used);
                text_opt = pushToArray(arr, text[i + 2], text_opt.used);

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
                while (j < (n - 2)
                       && !isControl(text[j])
                       && !isInText(text[j], reg))
                {
                    printf("[USECASE #44] %lu: %s\n", j, text[j]);
                    j += 1;
                }
                snprintf(snp_buf1, sizeof(snp_buf1), "lda.b %s", reg);
                snprintf(snp_buf2, sizeof(snp_buf2), "sta %s", local);
                if (matchString(text[j], snp_buf1)
                    && matchString(text[j + 1], snp_buf2))
                {
                    while (i < j)
                    {
                        text_opt = pushToArray(arr, text[i], text_opt.used);

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

        } // End of startWith(text[i], "ld")

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

                text_opt = pushToArray(arr, text[i + 2], text_opt.used);
                text_opt = pushToArray(arr, text[i + 3], text_opt.used);
                text_opt = pushToArray(arr, text[i], text_opt.used);
                text_opt = pushToArray(arr, text[i + 1], text_opt.used);

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
        if (matchString(text[i], "ldx #1")
            && startWith(text[i + 1], "lda.b tcc__")
            && matchString(text[i + 2], "sec")
            && startWith(text[i + 3], "sbc #")
            && matchString(text[i + 4], "tay")
            && matchString(text[i + 5], "beq +")
            && matchString(text[i + 6], "dex")
            && matchString(text[i + 7], "+")
            && startWith(text[i + 8], "stx.b tcc__")
            && matchString(text[i + 9], "txa")
            && matchString(text[i + 10], "bne +")
            && startWith(text[i + 11], "brl ")
            && matchString(text[i + 12], "+")
            && !matchString(text[i + 13], "tya"))
        {
            printf("[USECASE #47] %lu: %s\n", i, text[i]);

            text_opt = pushToArray(arr, text[i + 1], text_opt.used);

            char *ins = sliceStr(text[i + 3], 5, strlen(text[i + 1]));
            snprintf(snp_buf1, sizeof(snp_buf1), "cmp #%s", ins);
            text_opt = pushToArray(arr, snp_buf1, text_opt.used);

            text_opt = pushToArray(arr, text[i + 5], text_opt.used);
            text_opt = pushToArray(arr, text[i + 11], text_opt.used); // brl
            text_opt = pushToArray(arr, text[i + 12], text_opt.used); // +

            free(ins);

            i += 13;
            opted += 1;
            continue;
        }

        i++;

    } // End of while (i < n)

    if (text_opt.used > 0)
    {
        printf("\n\n______________[ASM CODE]_________________\n");
        for (size_t i = 0; i < text_opt.used; i++)
        {
            printf("%s\n", text_opt.arr[i]);
        }
        freedynArray(text_opt.arr, text_opt.used);
    }
    else
    {
        printf("\n\n______________[ASM CODE]_________________\n");
        free(arr);
    }
}
