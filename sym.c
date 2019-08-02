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
#include "symerr.h"

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

    dwarfinfo->di_compunits = linkedlist_new();
    dwarfinfo->di_numcompunits = 0;

    if(cu_load_compilation_units(dwarfinfo, error))
        return 1;

    *_dwarfinfo = dwarfinfo;

    return 0;
}

void sym_end(dwarfinfo_t **_dwarfinfo){
    if(!_dwarfinfo || !(*_dwarfinfo))
        return;

    dwarfinfo_t *dwarfinfo = *_dwarfinfo;

    struct node_t *current = dwarfinfo->di_compunits->front;

    while(current){
        void *cu = current->data;
        void *root_die = cu_get_root_die(cu);

        current = current->next;

        die_tree_free(dwarfinfo->di_dbg, root_die, 0);
        linkedlist_delete(dwarfinfo->di_compunits, cu);
        cu_free(cu);
    }

    close(dwarfinfo->di_fd);

    Dwarf_Error d_error = NULL;
    int ret = dwarf_finish(dwarfinfo->di_dbg, &d_error);

    linkedlist_free(dwarfinfo->di_compunits);
    free(dwarfinfo);
}

void sym_display_compilation_units(dwarfinfo_t *dwarfinfo){
    cu_display_compilation_units(dwarfinfo);
}

void *sym_find_compilation_unit_by_name(dwarfinfo_t *dwarfinfo, char *name){
    return cu_find_compilation_unit_by_name(dwarfinfo, name);
}

void *sym_get_compilation_unit_root_die(void *cu){
    return cu_get_root_die(cu);
}

void sym_display_die(void *die){
    die_display(die);
}

void sym_display_die_tree_starting_from(void *die){
    die_display_die_tree_starting_from(die);
}

void *sym_find_die_by_name(void *cu, const char *name){
    void *root_die = cu_get_root_die(cu);
    void *result = NULL;
    die_search(root_die, (void *)name, DIE_SEARCH_IF_NAME_MATCHES, &result);

    return result;
}

void *sym_find_function_die_by_pc(void *cu, uint64_t pc){
    void *root_die = cu_get_root_die(cu);
    void *result = NULL;
    die_search(root_die, (void *)pc, DIE_SEARCH_FUNCTION_BY_PC, &result);

    return result;
}

char *sym_get_die_name(void *die){
    return die_get_name(die);
}

uint64_t sym_get_die_high_pc(void *die){
    return die_get_high_pc(die);
}

uint64_t sym_get_die_low_pc(void *die){
    return die_get_low_pc(die);
}

void **sym_get_function_die_parameters(void *die, int *len){
    return die_get_parameters(die, len);
}

int sym_get_variable_dies(dwarfinfo_t *dwarfinfo, uint64_t pc,
        void ***vardies, int *len){
    void *cu = cu_find_compilation_unit_by_pc(dwarfinfo, pc);

    if(!cu)
        return 1;

    void *fxndie = sym_find_function_die_by_pc(cu, pc);

    if(!fxndie)
        return 1;

    return die_get_variables(dwarfinfo->di_dbg, fxndie, vardies, len);
}

void *sym_get_line_info_from_pc(dwarfinfo_t *dwarfinfo, uint64_t pc,
        char **outsrcfilename, char **outsrcfunction,
        uint64_t *outsrcfilelineno){
    void *cu = cu_find_compilation_unit_by_pc(dwarfinfo, pc);

    if(!cu)
        return NULL;

    void *root_die = cu_get_root_die(cu);

    die_get_line_info_from_pc(dwarfinfo->di_dbg, root_die, pc,
            outsrcfilename, outsrcfunction, outsrcfilelineno);

    return root_die;
}

void *sym_get_pc_of_next_line(dwarfinfo_t *dwarfinfo, uint64_t pc, uint64_t *next_line_pc){
    void *cu = cu_find_compilation_unit_by_pc(dwarfinfo, pc);

    if(!cu)
        return NULL;

    void *root_die = cu_get_root_die(cu);

    die_get_pc_of_next_line_a(dwarfinfo->di_dbg, root_die, pc, next_line_pc);

    return root_die;
}

int sym_get_pc_values_from_lineno(dwarfinfo_t *dwarfinfo, void *cu,
        uint64_t lineno, uint64_t **pcs, int *len){
    if(!dwarfinfo)
        return 1;

    return die_get_pc_values_from_lineno(dwarfinfo->di_dbg,
            cu_get_root_die(cu), lineno, pcs, len);
}

uint64_t sym_lineno_to_pc_a(dwarfinfo_t *dwarfinfo,
        char *srcfilename, uint64_t *srcfilelineno){
    void *cu = cu_find_compilation_unit_by_name(dwarfinfo, srcfilename);

    if(!cu)
        return 0;

    return die_lineno_to_pc(dwarfinfo->di_dbg, cu_get_root_die(cu), srcfilelineno);
}

uint64_t sym_lineno_to_pc_b(dwarfinfo_t *dwarfinfo, void *cu,
        uint64_t *srcfilelineno){
    return die_lineno_to_pc(dwarfinfo->di_dbg, cu_get_root_die(cu), srcfilelineno);
}

int sym_pc_to_lineno_a(dwarfinfo_t *dwarfinfo, uint64_t pc, uint64_t *srcfilelineno){
    void *cu = cu_find_compilation_unit_by_pc(dwarfinfo, pc);

    if(!cu)
        return 1;

    return die_pc_to_lineno(dwarfinfo->di_dbg, cu_get_root_die(cu), pc, srcfilelineno);
}

int sym_pc_to_lineno_b(dwarfinfo_t *dwarfinfo, void *cu, uint64_t pc, uint64_t *srcfilelineno){
    return die_pc_to_lineno(dwarfinfo->di_dbg, cu_get_root_die(cu), pc, srcfilelineno);
}

const char *sym_strerror(sym_error_t *e){
    return errmsg(e);
}
