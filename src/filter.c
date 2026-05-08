#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "filter.h"

#define MAX_RULES 256
#define MAX_RULE_LEN 256

static char advanced_rules[MAX_RULES][MAX_RULE_LEN];
static int  advanced_count = 0;
static int  rules_loaded = 0;

static void load_rules_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_RULE_LEN];
    while (fgets(line, sizeof(line), f) && advanced_count < MAX_RULES) {
        /* strip newline */
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;
        strncpy(advanced_rules[advanced_count], line, MAX_RULE_LEN - 1);
        advanced_rules[advanced_count][MAX_RULE_LEN - 1] = '\0';
        advanced_count++;
    }
    fclose(f);
}

static void ensure_rules_loaded(void) {
    if (rules_loaded) return;
    rules_loaded = 1;
    load_rules_file("rules/simple_patterns.txt");
    load_rules_file("rules/blocked_urls.txt");
}

int filter_simple(const char *request) {
    static const char *patterns[] = {
        "<script", "../", "OR 1=1", "DROP TABLE",
        "UNION SELECT", "alert(", "javascript:", "exec("
    };
    for (int i = 0; i < (int)(sizeof(patterns)/sizeof(patterns[0])); i++) {
        if (strcasestr(request, patterns[i]))
            return 1;
    }
    return 0;
}

int filter_advanced(const char *request) {
    ensure_rules_loaded();
    for (int i = 0; i < advanced_count; i++) {
        if (strcasestr(request, advanced_rules[i]))
            return 1;
    }
    return 0;
}

int should_block(const char *request) {
    return filter_simple(request) || filter_advanced(request);
}
