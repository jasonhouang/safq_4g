#ifndef __PARSE_CMD_LINE_H__
#define __PARSE_CMD_LINE_H__

int find_command_para(char *command_body, char para, char *para_body, int para_body_len);

void covert_line(char *cmdbuf, char *cmdname);

#endif

