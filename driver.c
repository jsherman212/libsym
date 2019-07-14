#include <stdio.h>
#include <stdlib.h>

#include "sym.h"

int main(int argc, char **argv, const char **envp){
    if(argc < 2 || !argv[1]){
        puts("need a dwarf file");
        return 1;
    }

    char *file = argv[1];
    char *error = NULL;

    void *dwarfinfo = NULL;

    if(sym_init_with_dwarf_file(file, &dwarfinfo, &error))
        printf("error: %s\n", error);

    free(error);
    
    sym_end(&dwarfinfo);

    return 0;
}
