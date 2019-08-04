#ifndef _SYM_H_
#define _SYM_H_

#include "symerr.h"

/*
 * Almost all of these functions return 0 on success and non-zero on error.
 * Those that do take a pointer to an error structure as the last parameter,
 * which can be passed to sym_strerror for a description of
 * what went wrong.
 * If an error is to be ignored, passing NULL in place of the error pointer
 * is always acceptable.
 */

/* General purpose functions */
int sym_init_with_dwarf_file(
        const char *    /* dSYM file path */, 
        void **         /* return dwarfinfo ptr */,
        void *          /* return error ptr */);

void sym_end(
        void **     /* dwarfinfo ptr */);


/* Compilation unit related functions */
int sym_display_compilation_units(
        void *      /* dwarfinfo ptr */,
        void *      /* return error ptr */);

int sym_find_compilation_unit_by_name(
        void *      /* dwarfinfo ptr */,
        void **     /* return CU ptr */,
        char *      /* name */,
        void *      /* return error ptr */);

int sym_get_compilation_unit_root_die(
        void *      /* compilation unit */,
        void **     /* reutrn root DIE */,
        void *      /* return error ptr */);


/* DIE related functions */
void sym_display_die(
        void *      /* die */);

void sym_display_die_tree_starting_from(
        void *      /* die */);

int sym_find_die_by_name(
        void *          /* compilation unit */,
        const char *    /* name */,
        void **         /* return die */,
        void *          /* return error ptr */);

int sym_find_function_die_by_pc(
        void *      /* compilation unit */,
        uint64_t    /* pc */,
        void **     /* return die */,
        void *      /* return error ptr */);

int sym_get_die_name(
        void *      /* die */,
        char **     /* return die name */,
        void *      /* return error ptr */);

int sym_get_die_high_pc(
        void *      /* die */,
        uint64_t *  /* return high pc */,
        void *      /* return error ptr */);

int sym_get_die_low_pc(
        void *      /* die */,
        uint64_t *  /* return low pc */,
        void *      /* return error ptr */);

/* Returns an array of parameter DIEs. Contents of the array must not
 * be freed, rather the array itself should be.
 */
int sym_get_function_die_parameters(
        void *      /* die */,
        void ***    /* return parameter array */,
        int *       /* return parameter array length */,
        void *      /* return error ptr */);

/* This function will search for a function DIE, based on pc, and return
 * an array with DIEs tagged DW_TAG_variable.
 */
int sym_get_variable_dies(
        void *      /* dwarfinfo ptr */,
        uint64_t    /* pc */,
        void ***    /* return array of variable DIEs */,
        int *       /* return array of variable DIEs len */,
        void *      /* return error ptr */);


/* Line related functions */

/* Returns CU DIE which this line resides in */
int sym_get_line_info_from_pc(
        void *      /* dwarfinfo ptr */,
        uint64_t    /* pc */,
        char **     /* return srcfilename */,
        char **     /* return srcfunction */,
        uint64_t *  /* return srcfilelineno */,
        void **     /* return CU DIE */,
        void *      /* return error ptr */);

/* This version takes in the dwarfinfo pointer.
 * It returns the CU DIE which the line resides in.
 */
int sym_get_pc_of_next_line(
        void *      /* dwarfinfo ptr */,
        uint64_t    /* starting PC */,
        uint64_t *  /* return next line PC */,
        void **     /* return CU DIE */,
        void *      /* return error ptr */);

/* Returns an array of the PC values a source line is comprised of. */
int sym_get_pc_values_from_lineno(
        void *          /* dwarfinfo ptr */,
        void *          /* compilation unit */,
        uint64_t        /* lineno */,
        uint64_t **     /* return PC values */,
        int *           /* return PC values array len */,
        void *          /* return error ptr */);

/* Finds CU DIE based on srcfilename */
int sym_lineno_to_pc_a(
        void *      /* dwarfinfo ptr */,
        char *      /* srcfilename */,
        uint64_t *  /* return srcfilelineno actually used */,
        uint64_t *  /* return PC */,
        void *      /* return error ptr */);

/* CU DIE given as second argument */
int sym_lineno_to_pc_b(
        void *      /* dwarfinfo ptr */,
        void *      /* compilation unit */,
        uint64_t *  /* return srcfilelineno actually used */,
        uint64_t *  /* return PC */,
        void *      /* return error ptr */);

/* Finds CU DIE based on srcfilename */
int sym_pc_to_lineno_a(
        void *      /* dwarfinfo ptr */,
        char *      /* srcfilename */,
        uint64_t    /* pc */,
        uint64_t *  /* return lineno */,
        void *      /* return error ptr */);

/* CU DIE given as second argument */
int sym_pc_to_lineno_b(
        void *      /* dwarfinfo ptr */,
        void *      /* compilation unit */,
        uint64_t    /* pc */,
        uint64_t *  /* return lineno */,
        void *      /* return error ptr */);


/* Error handling functions */
const char *sym_strerror(
        sym_error_t     /* error */);

void sym_errclear(
        sym_error_t *   /* error ptr */);

#endif
