#ifndef HELPERS_H
#define HELPERS_H

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*!
 * @brief Max length of the line.
 */
#define MAXLEN_LINE 2048

/**
 * @struct dynArray
 * @brief Structure to store the return of BssStore and TidyFile.
 * @var dynArray::arr
 * Member 'arr' contains the array of strings.
 * @var dynArray::used
 * Member 'used' contains the number of strings (elements)
 * in the array.
 */
typedef struct dynArray
{
    char **arr;
    size_t used;
} dynArray;

/**
 * @struct regexdynArray
 * @brief Structure to store the return of regexMatchGroups.
 * @var regexdynArray::status
 * Member 'status' contains the status if the function.
 * 1 if the regexp matchs groups, or 0 if not.
 * @var regexdynArray::regexCompiled
 * Member 'regexCompiled' contains the adress of
 the compiled regex.
 * @var regexdynArray::groups
 * Member 'groups' contains the array of strings
 * of matched groups.
 */
typedef struct regexdynArray
{
    size_t status;
    char **groups;
    size_t used;
} regexdynArray;

void freedynArray(char **p, size_t n);
int matchString(const char *str1, const char *str2);
int startWith(const char *source, const char *prefix);
int endWith(const char *source, const char *prefix);
int isInText(const char *source, const char *pattern);
int findMin(const int a, const int b);
char *trimWhiteSpace(char *str);
char *sliceStr(char *str, int slice_from, int slice_to);
char *replaceStr(char *str, char *orig, char *rep);
char *splitStr(char *str, char *sep, size_t pos);
regexdynArray regexMatchGroups(char *source, char *regex, const size_t maxGroups);
dynArray pushToArray(char **arr, char *str, int used);

#endif