#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FALSE       0
#define MAX_LINE    1024
#define TRUE        1

static void processSwitch(void);

static char line[MAX_LINE];

int main(int argc, char *argv[])
    {
    char *cp;

    while (fgets(line, MAX_LINE, stdin))
        {
        if ((strcmp(line, "void *\n") == 0)
            || (strcmp(line, "static void *\n") == 0))
            {
            for (cp = line; *cp != '\n'; cp++)
                {
                }
            *cp = '\0';
            }
        fputs(line, stdout);
        for (cp = line; isspace(*cp); cp++)
            {
            }
        if ((*cp != '\0') && (strncmp(cp, "switch (", 8) == 0))
            {
            processSwitch();
            }
        }
    }

static void processSwitch(void)
    {
    char *cp;
    int  swIndent;

    //
    // First line assumed to be open brace of switch body
    //
    if (fgets(line, MAX_LINE, stdin) == NULL)
        {
        fputs("Invalid switch statement: '{' of body missing\n", stderr);
        exit(1);
        }
    //
    // Count number of blanks until '{'. A '}' preceded by the same number
    // of blanks is assumed to be the end of the switch body.
    //
    for (cp = line; isspace(*cp); cp++)
        {
        }
    if (*cp != '{')
        {
        fputs("Invalid switch statement: '{' of body missing\n", stderr);
        exit(1);
        }
    fputs("    ", stdout);
    fputs(line, stdout);
    swIndent = cp - line;
    while (fgets(line, MAX_LINE, stdin))
        {
        for (cp = line; isspace(*cp); cp++)
            {
            }
        if (((strcmp(cp, "}\n") == 0) || (strcmp(cp, "}\r") == 0)) && ((cp - line) == swIndent))
            {
            fputs("    ", stdout);
            fputs(line, stdout);

            return;
            }
        if ((*cp != '\0') && (strncmp(cp, "switch (", 8) == 0))
            {
            fputs(line, stdout);
            processSwitch();
            continue;
            }
        fputs(line, stdout);
        }
    }
