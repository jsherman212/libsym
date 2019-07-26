#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libdwarf.h>

#include "common.h"
#include "die.h"
#include "linkedlist.h"

typedef struct {
    Dwarf_Unsigned cu_header_len;
    Dwarf_Unsigned cu_abbrev_offset;
    Dwarf_Half cu_address_size;
    Dwarf_Unsigned cu_next_header_offset;

    void *cu_root_die;
} compunit_t;

void cu_display_compilation_units(dwarfinfo_t *dwarfinfo){
    if(!dwarfinfo)
        return;

    int cnt = 1;

    LL_FOREACH(dwarfinfo->di_compunits, current){
        compunit_t *cu= current->data;

        char *cuname = die_get_name(cu->cu_root_die);

        printf("Compilation unit %d/%d:\n"
                "\tcu_header_len: %#llx\n"
                "\tcu_abbrev_offset: %#llx\n"
                "\tcu_address_size: %#x\n"
                "\tcu_next_header_offset: %#llx\n"
                "\tcu_diename: '%s'\n",
                cnt++, dwarfinfo->di_numcompunits,
                cu->cu_header_len, cu->cu_abbrev_offset,
                cu->cu_address_size, cu->cu_next_header_offset,
                cuname);
    }
}

compunit_t *cu_find_compilation_unit_by_name(dwarfinfo_t *dwarfinfo, char *name){
    if(!dwarfinfo || !name)
        return NULL;

    LL_FOREACH(dwarfinfo->di_compunits, current){
        compunit_t *cu = current->data;

        char *cuname = die_get_name(cu->cu_root_die);

        if(strcmp(cuname, name) == 0)
            return cu;
    }

    return NULL;
}

compunit_t *cu_find_compilation_unit_by_pc(dwarfinfo_t *dwarfinfo, uint64_t pc){
    if(!dwarfinfo)
        return NULL;

    LL_FOREACH(dwarfinfo->di_compunits, current){
        compunit_t *cu = current->data;

        Dwarf_Unsigned root_die_low_pc = die_get_low_pc(cu->cu_root_die);
        Dwarf_Unsigned root_die_high_pc = die_get_high_pc(cu->cu_root_die);
        
        if(pc >= root_die_low_pc && pc <= root_die_high_pc)
            return cu;
    }

    return NULL;
}

void cu_free(compunit_t *cu){
    if(!cu)
        return;

    free(cu->cu_root_die);
    free(cu);
}

Dwarf_Half cu_get_address_size(compunit_t *cu){
    if(!cu)
        return 0;

    return cu->cu_address_size;
}

void *cu_get_root_die(compunit_t *cu){
    if(!cu)
        return NULL;

    return cu->cu_root_die;
}

int cu_load_compilation_units(dwarfinfo_t *dwarfinfo, char **error){
    for(;;){
        compunit_t *cu = calloc(1, sizeof(compunit_t));
        Dwarf_Half ver, len_sz, ext_sz, hdr_type;
        Dwarf_Sig8 sig;
        Dwarf_Unsigned typeoff;
        Dwarf_Error d_error;
        int is_info = 1;

        int ret = dwarf_next_cu_header_d(dwarfinfo->di_dbg,
                is_info, &cu->cu_header_len, &ver, &cu->cu_abbrev_offset,
                &cu->cu_address_size, &len_sz, &ext_sz, &sig,
                &typeoff, &cu->cu_next_header_offset, &hdr_type,
                &d_error);

        if(ret == DW_DLV_ERROR){
            dwarf_dealloc(dwarfinfo->di_dbg, d_error, DW_DLA_ERROR);
            asprintf(error, "dwarf_next_cu_header_d: %s",
                    dwarf_errmsg_by_number(ret));
            return 1;
        }

        if(ret == DW_DLV_NO_ENTRY){
            dprintf("no more compilation units\n");
            free(cu);
            return 0;
        }

        void *root_die = NULL;
        initialize_and_build_die_tree_from_root_die(dwarfinfo, cu,
                &root_die, error);
        
        if(*error)
            return 1;

        cu->cu_root_die = root_die;

        linkedlist_add(dwarfinfo->di_compunits, cu);

        dwarfinfo->di_numcompunits++;
    }

    return 0;
}
