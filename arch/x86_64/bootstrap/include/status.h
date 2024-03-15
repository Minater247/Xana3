#ifndef _STATUS_H
#define _STATUS_H

#define STATUS(msg, ...) do { \
    printf("[WAIT] "); \
    printf(msg, ##__VA_ARGS__); \
    printf("\r"); \
} while(0)

#define DONE(msg, ...) do { \
    printf("[\033[1;32mOK\033[0m]\n"); \
    printf("]\n"); \
} while(0)

#define FAIL(msg, ...) do { \
    printf("[\033[1;91mFAIL\033[0m\n"); \
    printf(msg, ##__VA_ARGS__); \
    printf("\n"); \
} while(0)

#endif