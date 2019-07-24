#ifndef _SYM_H_
#define _SYM_H_

/* General purpose functions */
int sym_init_with_dwarf_file(const char *, void **, char **);
void sym_end(void **);

/* Compilation unit related functions */
void sym_display_compilation_units(void *);
void *sym_find_compilation_unit_by_name(void *, char *);
void *sym_get_compilation_unit_root_die(void *);

/* DIE related functions */
void sym_describe_die(void *);
void sym_display_die_tree_starting_from(void *);
void *sym_find_die_by_name(void *, const char *);
void *sym_find_function_die_by_pc(void *, uint64_t);
char *sym_get_die_name(void *);

#endif
