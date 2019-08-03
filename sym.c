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

int sym_init_with_dwarf_file(const char *file, dwarfinfo_t **_dwarfinfo,
        sym_error_t *e){
    int fd = open(file, O_RDONLY);

    if(fd < 0){
        errset(e, GENERIC_ERROR_KIND, GE_FILE_NOT_FOUND);
        return 1;
    }

    dwarfinfo_t *dwarfinfo = calloc(1, sizeof(dwarfinfo_t));
    Dwarf_Error d_error = NULL;
    int ret = dwarf_init(fd, DW_DLC_READ, NULL, NULL,
            &dwarfinfo->di_dbg, &d_error);

    if(ret != DW_DLV_OK){
        errset(e, SYM_ERROR_KIND, SYM_DWARF_INIT_FAILED);
        free(dwarfinfo);
        return 1;
    }

    dwarfinfo->di_compunits = linkedlist_new();
    dwarfinfo->di_numcompunits = 0;

    if(cu_load_compilation_units(dwarfinfo, e))
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
        void *root_die = NULL;

        cu_get_root_die(cu, &root_die, NULL);

        current = current->next;

        die_tree_free(dwarfinfo->di_dbg, root_die, 0);
        linkedlist_delete(dwarfinfo->di_compunits, cu);
        cu_free(cu, NULL);
    }

    close(dwarfinfo->di_fd);

    Dwarf_Error d_error = NULL;
    int ret = dwarf_finish(dwarfinfo->di_dbg, &d_error);

    linkedlist_free(dwarfinfo->di_compunits);
    free(dwarfinfo);
}

int sym_display_compilation_units(dwarfinfo_t *dwarfinfo,
        sym_error_t *e){
    return cu_display_compilation_units(dwarfinfo, e);
}

int sym_find_compilation_unit_by_name(dwarfinfo_t *dwarfinfo, void **cuout,
        char *name, sym_error_t *e){
    return cu_find_compilation_unit_by_name(dwarfinfo, cuout, name, e);
}

int sym_get_compilation_unit_root_die(void *cu, void **dieout,
        sym_error_t *e){
    return cu_get_root_die(cu, dieout, e);
}

void sym_display_die(void *die){
    die_display(die);
}

void sym_display_die_tree_starting_from(void *die){
    die_display_die_tree_starting_from(die);
}

int sym_find_die_by_name(void *cu, const char *name, void **dieout,
        sym_error_t *e){
    void *root_die = NULL;
    if(cu_get_root_die(cu, &root_die, e))
        return 1;

    void *result = NULL;
    die_search(root_die, (void *)name, DIE_SEARCH_IF_NAME_MATCHES, &result);

    *dieout = result;
    return 0;
}

int sym_find_function_die_by_pc(void *cu, uint64_t pc, void **dieout,
        sym_error_t *e){
    void *root_die = NULL;
    if(cu_get_root_die(cu, &root_die, e))
        return 1;

    void *result = NULL;
    die_search(root_die, (void *)pc, DIE_SEARCH_FUNCTION_BY_PC, &result);

    *dieout = result;
    return 0;
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
        void ***vardies, int *len, sym_error_t *e){
    void *cu = NULL;
    if(cu_find_compilation_unit_by_pc(dwarfinfo, &cu, pc, e))
        return 1;

    void *fxndie = NULL;
    if(sym_find_function_die_by_pc(cu, pc, &fxndie, e))
        return 1;

    die_get_variables(dwarfinfo->di_dbg, fxndie, vardies, len);

    return 0;
}

int sym_get_line_info_from_pc(dwarfinfo_t *dwarfinfo, uint64_t pc,
        char **outsrcfilename, char **outsrcfunction,
        uint64_t *outsrcfilelineno, void **cudieout, sym_error_t *e){
    void *cu = NULL;
    if(cu_find_compilation_unit_by_pc(dwarfinfo, &cu, pc, e))
        return 1;

    void *root_die = NULL;
    if(cu_get_root_die(cu, &root_die, e))
        return 1;

    die_get_line_info_from_pc(dwarfinfo->di_dbg, root_die, pc,
            outsrcfilename, outsrcfunction, outsrcfilelineno);

    *cudieout = root_die;
    return 0;
}

int sym_get_pc_of_next_line(dwarfinfo_t *dwarfinfo, uint64_t pc,
        uint64_t *next_line_pc, void **cudieout, sym_error_t *e){
    void *cu = NULL;
    if(cu_find_compilation_unit_by_pc(dwarfinfo, &cu, pc, e))
        return 1;

    void *root_die = NULL;
    if(cu_get_root_die(cu, &root_die, e))
        return 1;

    die_get_pc_of_next_line_a(dwarfinfo->di_dbg, root_die, pc, next_line_pc);

    *cudieout = root_die;
    return 0;
}

int sym_get_pc_values_from_lineno(dwarfinfo_t *dwarfinfo, void *cu,
        uint64_t lineno, uint64_t **pcs, int *len, sym_error_t *e){
    if(!dwarfinfo){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DWARFINFO);
        return 1;
    }

    void *root_die = NULL;
    if(cu_get_root_die(cu, &root_die, e))
        return 1;

    return die_get_pc_values_from_lineno(dwarfinfo->di_dbg, root_die, lineno,
            pcs, len);
}

int sym_lineno_to_pc_a(dwarfinfo_t *dwarfinfo,
        char *srcfilename, uint64_t *srcfilelineno, uint64_t *pcout,
        sym_error_t *e){
    void *cu = NULL;
    if(cu_find_compilation_unit_by_name(dwarfinfo, &cu, srcfilename, e))
        return 1;

    void *root_die = NULL;
    if(cu_get_root_die(cu, &root_die, e))
        return 1;

    *pcout = die_lineno_to_pc(dwarfinfo->di_dbg, root_die, srcfilelineno);
    return 0;
}

int sym_lineno_to_pc_b(dwarfinfo_t *dwarfinfo, void *cu,
        uint64_t *srcfilelineno, uint64_t *pcout, sym_error_t *e){
    void *root_die = NULL;
    if(cu_get_root_die(cu, &root_die, e))
        return 1;

    *pcout = die_lineno_to_pc(dwarfinfo->di_dbg, root_die, srcfilelineno);
    return 0;
}

int sym_pc_to_lineno_a(dwarfinfo_t *dwarfinfo, uint64_t pc,
        uint64_t *srcfilelineno, sym_error_t *e){
    void *cu = NULL;
    if(cu_find_compilation_unit_by_pc(dwarfinfo, &cu, pc, e))
        return 1;

    void *root_die = NULL;
    if(cu_get_root_die(cu, &root_die, e))
        return 1;

    return die_pc_to_lineno(dwarfinfo->di_dbg, root_die, pc, srcfilelineno);
}

int sym_pc_to_lineno_b(dwarfinfo_t *dwarfinfo, void *cu, uint64_t pc,
        uint64_t *srcfilelineno, sym_error_t *e){
    void *root_die = NULL;
    if(cu_get_root_die(cu, &root_die, e))
        return 1;

    return die_pc_to_lineno(dwarfinfo->di_dbg, root_die, pc, srcfilelineno);
}

const char *sym_strerror(sym_error_t e){
    return errmsg(e);
}

void sym_errclear(sym_error_t *e){
    errclear(e);
}
