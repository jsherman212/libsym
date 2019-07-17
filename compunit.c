#include <stdio.h>
#include <stdlib.h>

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
    char **cu_srcfiles;
    Dwarf_Signed cu_srcfilecnt;
} compunit_t;

/*
static compunit_t *create_compile_unit(Dwarf_Unsigned hdrlen,
        Dwarf_Unsigned abbrev_off, Dwarf_Half addrsz){
    compunit_t *unit = malloc(sizeof(compunit_t));
    unit->cu_header_len = hdrlen;
    unit->cu_abbrev_offset = abbrev_off;
    unit->cu_address_size = addrsz;

    return unit;
}
*/

static void load_child_dies(dwarfinfo_t *dwarfinfo, char **error){

}

int sym_load_compilation_units(dwarfinfo_t *dwarfinfo, char **error){
    for(;;){
        compunit_t *unit = malloc(sizeof(compunit_t));
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

void display_compilation_units(dwarfinfo_t *dwarfinfo){
    int cnt = 1;
    LL_FOREACH(dwarfinfo->di_compunits, current){
        compunit_t *unit = current->data;

        /*
        printf("Compilation unit %d/%d:\n"
                "\tcu_header_len: %#llx\n"
                "\tcu_abbrev_offset: %#llx\n"
                "\tcu_address_size: %#x\n"
                "\tcu_next_header_offset: %#llx\n"
                "\tcu_diename: '%s'\n" 
                "\tcu_srcfilecnt: %lld\n",
                cnt++, dwarfinfo->di_numcompunits,
                unit->cu_header_len, unit->cu_abbrev_offset,
                unit->cu_address_size, unit->cu_next_header_offset,
                unit->cu_diename?unit->cu_diename:"NULL",
                unit->cu_srcfilecnt);
                */
        printf("Compilation unit %d/%d:\n"
                "\tcu_header_len: %#llx\n"
                "\tcu_abbrev_offset: %#llx\n"
                "\tcu_address_size: %#x\n"
                "\tcu_next_header_offset: %#llx\n",
                cnt++, dwarfinfo->di_numcompunits,
                unit->cu_header_len, unit->cu_abbrev_offset,
                unit->cu_address_size, unit->cu_next_header_offset);
        printf("\tdebugging information entries:\n");
        display_from_root_die(unit->cu_root_die);
        putchar('\n');
        
        
        /*
        char *desc = NULL;
        
        asprintf(&desc, "Compilation unit %d/%d:\n"
                "\tcu_header_len: %#llx\n"
                "\tcu_abbrev_offset: %#llx\n"
                "\tcu_address_size: %#x\n"
                "\tcu_next_header_offset: %#llx\n",
                cnt++, dwarfinfo->di_numcompunits,
                unit->cu_header_len, unit->cu_abbrev_offset,
                unit->cu_address_size, unit->cu_next_header_offset);
        asprintf(&desc, "\tdebugging information entries:\n");

        asprintf(&desc, "Compilation unit %d/%d:\n", cnt++, dwarfinfo->di_numcompunits);
        printf("%s", desc);

        free(desc);
        */
    }
}

Dwarf_Half compunit_get_address_size(compunit_t *unit){
    if(!unit)
        return 0;

    return unit->cu_address_size;
}
