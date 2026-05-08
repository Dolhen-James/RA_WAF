#ifndef LOGGER_H
#define LOGGER_H

void logger_init(const char *filepath);
void logger_log(const char *client_ip, const char *first_line, int blocked);
void logger_close(void);

#endif
