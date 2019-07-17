#ifndef _COMPUNIT_H_
#define _COMPUNIT_H_

#include <libdwarf.h>

int sym_load_compilation_units(void *, char **);
Dwarf_Half compunit_get_address_size(void *);
void display_compilation_units(void *);

#endif
