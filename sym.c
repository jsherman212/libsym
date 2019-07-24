#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "compunit.h"
#include "die.h"
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

    //dprintf("_E %d\n", _E);

    dwarfinfo->di_compunits = linkedlist_new();
    dwarfinfo->di_numcompunits = 0;

    if(cu_load_compilation_units(dwarfinfo, error))
        return 1;

    //display_compilation_units(dwarfinfo);

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

void sym_display_compilation_units(dwarfinfo_t *dwarfinfo){
    if(!dwarfinfo)
        return;

    cu_display_compilation_units(dwarfinfo);
}

void *sym_find_compilation_unit_by_name(dwarfinfo_t *dwarfinfo, char *name){
    if(!dwarfinfo || !name)
        return NULL;

    return cu_find_compilation_unit_by_name(dwarfinfo, name);
}

void *sym_get_compilation_unit_root_die(void *unit){
    return cu_get_root_die(unit);
}

void sym_describe_die(void *die){
    die_describe(die);
}

void sym_display_die_tree_starting_from(void *die){
    die_display_die_tree_starting_from(die);
}

void *sym_find_die_by_name(void *unit, const char *name){
    void *root_die = cu_get_root_die(unit);
    void *result = NULL;
    die_find_by_name(root_die, name, &result);

    return result;
}

void *sym_find_function_die_by_pc(void *unit, uint64_t pc){
    void *root_die = cu_get_root_die(unit);
    void *result = NULL;
    die_find_function_die_by_pc(root_die, pc, &result);

    return result;
}

char *sym_get_die_name(void *die){
    return die_get_name(die);
}

