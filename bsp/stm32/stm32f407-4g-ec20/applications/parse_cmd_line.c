#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int find_command_para(char *command_body, char para, char *para_body, int para_body_len)
{
    int i = 0;
    int k, j = 0;
    int retval = -1;

    para = toupper(para);
    while (command_body[i] != 0)
    {
        if (command_body[i] == '-' && command_body[i+1] == para) {
            retval = 0;
            for (k = i + 2; command_body[k] == ' '; k++);
            for (j = 0; command_body[k] != ' ' && command_body[k] != 0 && command_body[k] != '-'; j++, k++) {
                para_body[j] = command_body[k];
                retval ++;
                if (retval == para_body_len)
                    return retval;
            }
        }
        i ++;
    }

    return retval;
}

void covert_line(char *cmdbuf, char *cmdname)
{
    int i, j;

    for (i = 0; cmdbuf[i] == ' '; i++);        /* skip blanks on head         */

    for (; cmdbuf[i] != 0; i++)  {             /* convert to upper characters */
        cmdbuf[i] = toupper(cmdbuf[i]);
    }

    for (i = 0; cmdbuf[i] == ' '; i++);        /* skip blanks on head         */
    for (j = 0; cmdbuf[i] != ' ' && cmdbuf[i] != 0; i++, j++)  {         /* find command name       */
        cmdname[j] = cmdbuf[i];
    }
    cmdname[j] = '\0';
}
