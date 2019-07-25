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
        compunit_t *unit = current->data;

        char *cuname = die_get_name(unit->cu_root_die);

        printf("Compilation unit %d/%d:\n"
                "\tcu_header_len: %#llx\n"
                "\tcu_abbrev_offset: %#llx\n"
                "\tcu_address_size: %#x\n"
                "\tcu_next_header_offset: %#llx\n"
                "\tcu_diename: '%s'\n",
                cnt++, dwarfinfo->di_numcompunits,
                unit->cu_header_len, unit->cu_abbrev_offset,
                unit->cu_address_size, unit->cu_next_header_offset,
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

Dwarf_Half cu_get_address_size(compunit_t *unit){
    if(!unit)
        return 0;

    return unit->cu_address_size;
}

void *cu_get_root_die(compunit_t *unit){
    if(!unit)
        return NULL;

    return unit->cu_root_die;
}

int cu_load_compilation_units(dwarfinfo_t *dwarfinfo, char **error){
    for(;;){
        compunit_t *unit = calloc(1, sizeof(compunit_t));
        Dwarf_Half ver, len_sz, ext_sz, hdr_type;
        Dwarf_Sig8 sig;
        Dwarf_Unsigned typeoff;
        Dwarf_Error d_error;
        int is_info = 1;

        int ret = dwarf_next_cu_header_d(dwarfinfo->di_dbg,
                is_info, &unit->cu_header_len, &ver, &unit->cu_abbrev_offset,
                &unit->cu_address_size, &len_sz, &ext_sz, &sig,
                &typeoff, &unit->cu_next_header_offset, &hdr_type,
                &d_error);

        if(ret == DW_DLV_ERROR){
            asprintf(error, "dwarf_next_cu_header_d: %s",
                    dwarf_errmsg_by_number(ret));
            return 1;
        }

        if(ret == DW_DLV_NO_ENTRY){
            dprintf("no more compilation units\n");
            return 0;
        }

        void *root_die = NULL;
        initialize_and_build_die_tree_from_root_die(dwarfinfo, unit,
                &root_die, error);
        
        if(*error)
            return 1;

        unit->cu_root_die = root_die;

        linkedlist_add(dwarfinfo->di_compunits, unit);

        dwarfinfo->di_numcompunits++;
    }

    return 0;
}
