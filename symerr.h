#ifndef _SYMERR_H_
#define _SYMERR_H_

typedef struct {
    unsigned error_kind;
    unsigned error_id;
} sym_error_t;

enum {
    SYM_FILE_NOT_FOUND = 0,


};

const char *errmsg(sym_error_t *);
void errclear(sym_error_t *);

#endif
