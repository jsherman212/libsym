#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "linkedlist.h"
#include "stack.h"

#include <libdwarf.h>

typedef struct {
    /* File descriptor for our DWARF file */
    int di_fd;

    Dwarf_Debug di_dbg;


} dwarfinfo_t;

int sym_init_with_dwarf_file(const char *fname, dwarfinfo_t **dwarfinfo){


    return 0;
}
