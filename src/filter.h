#ifndef FILTER_H
#define FILTER_H

int filter_simple(const char *request);
int filter_advanced(const char *request);
int should_block(const char *request);

#endif
