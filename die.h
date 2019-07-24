#ifndef _DIE_H_
#define _DIE_H_

void die_describe(void *);
void die_display_die_tree_starting_from(void *);
char *die_get_name(void *);
uint64_t die_get_high_pc(void *);
uint64_t die_get_low_pc(void *);
void die_search(void *, void *, int, void *);
int initialize_and_build_die_tree_from_root_die(void *, void *, void **, char **);

#endif
