#include <stdlib.h>

#include "symerr.h"

static const char *const NO_ERROR_TABLE[] = {
    "No error (0)"
};

static const char *const GENERIC_ERROR_TABLE[] = {
    "No error (0)",
    "File not found (1)",
    "Invalid parameter (2)",
    "Invalid compilation unit pointer (3)",
    "Invalid dwarfinfo pointer (4)"

};

static const char *const SYM_ERROR_TABLE[] = {
    "No error (0)",
    "dwarf_init failed (1)"
};

static const char *const CU_ERROR_TABLE[] = {
    "No error (0)",
    "Compilation unit not found (1)",
    "dwarf_next_cu_header_d failed (2)"
};

static const char *const *const ERROR_TABLES[] = {
    NO_ERROR_TABLE,
    GENERIC_ERROR_TABLE,
    SYM_ERROR_TABLE,
    CU_ERROR_TABLE
};

void errclear(sym_error_t *e){
    if(!e)
        return;

    e->error_kind = 0;
    e->error_id = 0;
    e = NULL;
}

const char *errmsg(sym_error_t e){
    if(e.error_kind > sizeof(ERROR_TABLES) / sizeof(ERROR_TABLES[0]))
        return "sym_error_t error kind out of bounds";

    if(e.error_id > sizeof(ERROR_TABLES[e.error_kind]) / sizeof(const char *))
        return "sym_error_t error ID out of bounds";

    return ERROR_TABLES[e.error_kind][e.error_id];
}

void errset(sym_error_t *e, unsigned ekind, unsigned eid){
    if(!e)
        return;

    e->error_kind = ekind;
    e->error_id = eid;
}
