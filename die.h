#ifndef _DIE_H_
#define _DIE_H_

void die_display(void *);
void die_display_die_tree_starting_from(void *);
char *die_get_name(void *);
uint64_t die_get_high_pc(void *);
void die_get_line_info_from_pc(void *, void *, uint64_t, char **, char **, uint64_t *);
uint64_t die_get_low_pc(void *);
void **die_get_parameters(void *, int *);
void die_get_pc_of_next_line_a(void *, void *, uint64_t, uint64_t *);
int die_get_pc_values_from_lineno(void *, void *, uint64_t, uint64_t **, int *);
int die_get_variables(void *, void *, void ***, int *);
uint64_t die_lineno_to_pc(void *, void *, uint64_t *);
int die_pc_to_lineno(void *, void *, uint64_t, uint64_t *);
void die_search(void *, void *, int, void **);
void die_tree_free(void *, void *, int);

/* Internal functions */
int initialize_and_build_die_tree_from_root_die(void *, void *, void **, char **);

#endif
