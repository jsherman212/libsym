#ifndef _COMPUNIT_H_
#define _COMPUNIT_H_

#include <libdwarf.h>

void cu_display_compilation_units(void *);
void *cu_find_compilation_unit_by_name(void *, char *);
Dwarf_Half cu_get_address_size(void *);
void *cu_get_root_die(void *);
int cu_load_compilation_units(void *, char **);

#endif
