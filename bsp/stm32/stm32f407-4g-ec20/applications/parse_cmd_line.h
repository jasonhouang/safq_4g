#ifndef __PARSE_CMD_LINE_H__
#define __PARSE_CMD_LINE_H__

int find_command_para(char *command_body, char para, char *para_body, int para_body_len);

void covert_line(char *cmdbuf, char *cmdname);

uint32_t crc32_compute(uint8_t const * p_data, uint32_t size, uint32_t const * p_crc);

#endif

