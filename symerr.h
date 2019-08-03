#ifndef _SYMERR_H_
#define _SYMERR_H_

typedef struct {
    unsigned error_kind;
    unsigned error_id;
} sym_error_t;

enum {
    NO_ERROR_KIND = 0,
    GENERIC_ERROR_KIND,
    SYM_ERROR_KIND,
    CU_ERROR_KIND
};

enum {
    GE_NO_ERROR = 0,
    GE_FILE_NOT_FOUND,
    GE_INVALID_PARAMETER,
    GE_INVALID_CU_POINTER,
    GE_INVALID_DWARFINFO

};

enum {
    SYM_NO_ERROR = 0,
    SYM_DWARF_INIT_FAILED,

};

enum {
    CU_NO_ERROR = 0,
    CU_CU_NOT_FOUND,
    CU_DWARF_NEXT_CU_HEADER_D_FAILED,
};

void errclear(sym_error_t *);
const char *errmsg(sym_error_t);
void errset(sym_error_t *, unsigned, unsigned);

#endif
