#ifndef _SYM_H_
#define _SYM_H_

/*
 * All of these functions return 0 on success and non-zero on error.
 * All of them take a pointer to an error structure as the last parameter,
 * which can be passed to sym_strerror for a detailed description of
 * what went wrong.
 */

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

/* Returns an array of parameter DIEs. Contents of the array must not
 * be freed, rather the array itself should be.
 */
void **sym_get_function_die_parameters(
        void *      /* die */,
        int *       /* numparams */);

/* This function will search for a function DIE, based on pc, and return
 * an array with DIEs tagged DW_TAG_variable.
 */
int sym_get_variable_dies(
        void *      /* dwarfinfo ptr */,
        uint64_t    /* pc */,
        void ***    /* return array of variable DIEs */,
        int *       /* return array of variable DIEs len */);


/* Line related functions */

/* Returns CU DIE which this line resides in */
void *sym_get_line_info_from_pc(
        void *      /* dwarfinfo ptr */,
        uint64_t    /* pc */,
        char **     /* return srcfilename */,
        char **     /* return srcfunction */,
        uint64_t *  /* return srcfilelineno */);

/* This version takes in the dwarfinfo pointer.
 * It returns the CU DIE which the line resides in.
 */
void *sym_get_pc_of_next_line(
        void *      /* dwarfinfo ptr */,
        uint64_t    /* starting PC */,
        uint64_t *  /* return next line PC */);

/* Returns an array of the PC values a source line is comprised of. */
int sym_get_pc_values_from_lineno(
        void *          /* dwarfinfo ptr */,
        void *          /* compilation unit */,
        uint64_t        /* lineno */,
        uint64_t **     /* return PC values */,
        int *           /* return PC values array len */);

/* Finds CU DIE based on srcfilename */
uint64_t sym_lineno_to_pc_a(
        void *      /* dwarfinfo ptr */,
        char *      /* srcfilename */,
        uint64_t *  /* return srcfilelineno actually used */);

/* CU DIE given as second argument */
uint64_t sym_lineno_to_pc_b(
        void *      /* dwarfinfo ptr */,
        void *      /* compilation unit */,
        uint64_t *  /* return srcfilelineno actually used */);

/* Finds CU DIE based on srcfilename */
int sym_pc_to_lineno_a(
        void *      /* dwarfinfo ptr */,
        char *      /* srcfilename */,
        uint64_t    /* pc */,
        uint64_t *  /* return lineno */);

/* CU DIE given as second argument */
int sym_pc_to_lineno_b(
        void *      /* dwarfinfo ptr */,
        void *      /* compilation unit */,
        uint64_t    /* pc */,
        uint64_t *  /* return lineno */);


/* Error handling functions */
const char *sym_strerror(
        void *       /* error pointer */);

#endif
