#ifndef _COMMON_H_
#define _COMMON_H_

#include <libdwarf.h>

typedef struct {
    /* Our DWARF file */
    int di_fd;

    Dwarf_Debug di_dbg;

    struct linkedlist *di_compunits;
    int di_numcompunits;
} dwarfinfo_t;

#define dprintf(fmt, ...) do { \
    printf("%s:%s:%d: " fmt, __FILE__, __func__, __LINE__, ##__VA_ARGS__); \
    } while(0)

#define __bail(x) do { \
    puts(x); \
    exit(1); \
} while(0)

#define STRINGIFY(x) #x

#define LL_FOREACH(list, var) \
    for(struct node_t *var = list->front; \
            var; \
            var = var->next)

#endif
