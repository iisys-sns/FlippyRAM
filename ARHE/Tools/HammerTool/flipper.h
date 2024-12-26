#ifndef HTOOL_FLIPPER
#define HTOOL_FLIPPER
typedef struct {
    int id;
    int nHugePages;
    volatile char *buf;
    int sleep;
    int arm;
} flipperThreadItem;
#endif
