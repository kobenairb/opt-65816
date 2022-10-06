#include "header.h"

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
int StartWith(const char *a, const char *b)
{
    if (strncmp(a, b, strlen(b)) == 0)
        return 1;

    return 0;
}

/* get the smalest number between two int */
int findSmall(const int a, const int b)
{
    if (a > b)
        return b;
    else
        return a;
}

/* Check return code */
void CheckRc(const char *message, const int rc)
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

    // Trim leading space
    while (isspace((unsigned char)*str))
        str++;

    if (*str == 0) // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}
