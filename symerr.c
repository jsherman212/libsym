#include <stdlib.h>

#include "symerr.h"

static const char *const NO_ERROR_TABLE[] = {
    "No error (0)"
};

static const char *const *const ERROR_TABLES[] = {
    NO_ERROR_TABLE
};

const char *errmsg(sym_error_t *e){
    if(!e)
        return "sym_error_t parameter NULL";

    return ERROR_TABLES[e->error_kind][e->error_id];
}

void errclear(sym_error_t *e){
    if(!e)
        return;

    e->error_kind = 0;
    e->error_id = 0;
    e = NULL;
}
