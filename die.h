#ifndef _DIE_H_
#define _DIE_H_

void die_describe(void *);
void die_display_die_tree_starting_from(void *);
void *die_find_by_name(void *, const char *, void *);
void *die_find_function_die_by_pc(void *, uint64_t, void *);
char *die_get_name(void *);
int initialize_and_build_die_tree_from_root_die(void *, void *, void **, char **);

#endif
