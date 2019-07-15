#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "compunit.h"
#include "linkedlist.h"
#include "stack.h"

#include <libdwarf.h>

// XXX replace all asprintf with concat when done

int sym_init_with_dwarf_file(const char *file, dwarfinfo_t **_dwarfinfo,
        char **error){
    dwarfinfo_t *dwarfinfo = calloc(1, sizeof(dwarfinfo_t));

    int fd = open(file, O_RDONLY);

    if(fd < 0){
        asprintf(error, "could not open '%s'", file);
        return 1;
    }

    Dwarf_Error d_error;
    int _E = dwarf_init(fd, DW_DLC_READ, NULL, NULL,
            &dwarfinfo->di_dbg, &d_error);

    if(_E != DW_DLV_OK){
        asprintf(error, "dwarf_init: %s", dwarf_errmsg_by_number(_E));
        return 1;
    }

    dprintf("_E %d\n", _E);

    dwarfinfo->di_compunits = linkedlist_new();
    dwarfinfo->di_numcompunits = 0;

    if(sym_load_compilation_units(dwarfinfo, error))
        return 1;

    display_compilation_units(dwarfinfo);

    *_dwarfinfo = dwarfinfo;

    return 0;
}

void sym_end(dwarfinfo_t **_dwarfinfo){
    if(!_dwarfinfo || !(*_dwarfinfo))
        return;

    dwarfinfo_t *dwarfinfo = *_dwarfinfo;

    close(dwarfinfo->di_fd);

    Dwarf_Error error;
    dwarf_finish(dwarfinfo->di_dbg, &error);

    dprintf("here\n");
}
