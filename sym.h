#ifndef _SYM_H_
#define _SYM_H_

/* General purpose functions */
int sym_init_with_dwarf_file(
        const char *    /* dSYM file path */, 
        void **         /* return dwarfinfo ptr */,
        char **         /* return error */);

void sym_end(
        void **     /* dwarfinfo ptr */);


/* Compilation unit related functions */
void sym_display_compilation_units(
        void *      /* dwarfinfo ptr */);

void *sym_find_compilation_unit_by_name(
        void *      /* dwarfinfo ptr */,
        char *      /* name */);

void *sym_get_compilation_unit_root_die(
        void *      /* compilation unit */);


/* DIE related functions */
void sym_display_die(
        void *      /* die */);

void sym_display_die_tree_starting_from(
        void *      /* die */);

void *sym_find_die_by_name(
        void *          /* compilation unit */,
        const char *    /* name */);

void *sym_find_function_die_by_pc(
        void *      /* compilation unit */,
        uint64_t    /* pc */);

char *sym_get_die_name(
        void *      /* die */);

uint64_t sym_get_die_high_pc(
        void *      /* die */);

uint64_t sym_get_die_low_pc(
        void *      /* die */);

void **sym_get_function_die_parameters(
        void *      /* die */,
        int *       /* numparams */);

/* Line related functions */
/* Returns CU DIE which this line resides in */
void *sym_get_line_info_from_pc(
        void *      /* dwarfinfo ptr */,
        uint64_t    /* pc */,
        char **     /* return srcfilename */,
        char **     /* return srcfunction */,
        uint64_t *  /* return srcfilelineno */);

/* Finds CU DIE based on srcfilename */
uint64_t sym_lineno_to_pc_a(
        void *      /* dwarfinfo ptr */,
        char *      /* srcfilename */,
        uint64_t *  /* return srcfilelineno */);

/* CU DIE given as first argument */
uint64_t sym_lineno_to_pc_b(
        void *      /* compilation unit */,
        uint64_t *  /* return srcfilelineno */);

#endif
