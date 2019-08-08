#include <stdio.h>
#include <stdlib.h>

#include "sym.h"

int main(int argc, char **argv, const char **envp){
    if(argc < 2 || !argv[1]){
        puts("need a dwarf file");
        return 1;
    }

    char *file = argv[1];

    sym_error_t sym_error = {0};
    void *dwarfinfo = NULL;

    if(sym_init_with_dwarf_file(file, &dwarfinfo, &sym_error))
        printf("error: %s\n", sym_strerror(sym_error));

    errclear(&sym_error);

    int display_compile_unit_menu = 1;

    void *current_compile_unit = NULL;

    enum {
        CHOICE_CHOOSE_COMPILE_UNIT = 1,
        CHOICE_LIST_COMPILE_UNITS,
        CHOICE_RESET_CURRENT_COMPILE_UNIT,
        CHOICE_DISPLAY_DIE_TREE_FROM_ROOT,
        CHOICE_CONVERT_LINE_NUMBER_TO_PC,
        CHOICE_CONVERT_PC_TO_LINE_NUMBER,
        CHOICE_GET_PC_VALUES_FROM_LINE_NUMBER,
        CHOICE_GET_LINE_INFO_FROM_PC,
        CHOICE_GET_LINE_AFTER_PC,
        CHOICE_GET_VARIABLE_DIES_AROUND_PC,
        CHOICE_DISPLAY_DIE_MENU,
        CHOICE_QUIT
    };

    void *current_die = NULL;

    enum {
        CHOICE_FIND_DIE_BY_NAME = 1,
        CHOICE_FIND_FUNCTION_DIE_BY_PC,
        CHOICE_DISPLAY_CURRENT_DIE,
        CHOICE_GET_PARAMETERS_FROM_DIE,
        CHOICE_GET_STRUCT_OR_UNION_MEMBERS_FROM_DIE,
        CHOICE_RESET_CURRENT_DIE,
        CHOICE_DISPLAY_COMPILATION_UNIT_MENU
    };


    // XXX: do NOT use any function not preceded by 'sym_'.
    // Would defeat the purpose of the abstraction

    while(1){
        printf("Current menu:\n");
        if(display_compile_unit_menu){
            printf("There %s a compile unit chosen\n", current_compile_unit?"is":"isn't");
            printf("1. Choose compile unit\n"
                    "2. List compile units\n"
                    "3. Reset current compile unit\n"
                    "4. Display this compile unit root DIE tree\n"
                    "5. Convert line number to a virtual address in this CU\n"
                    "6. Convert PC to line number\n"
                    "7. Get the PC values a source line comprises of\n"
                    "8. Get line info from an arbitrary PC\n"
                    "9. Starting from an arbitrary PC, get the PC of the line right after\n"
                    "10. Display variables from a function, given an arbitrary PC\n"
                    "11. Display DIE menu\n"
                    "12. Quit\n");
            int choice = 0;
            scanf("%d", &choice);

            switch(choice){
                case CHOICE_CHOOSE_COMPILE_UNIT:
                    {
                        printf("Compile units:\n");
                        if(sym_display_compilation_units(dwarfinfo, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        printf("\nWhich? Enter name: ");
                        char name[200] = {0};
                        scanf("%s", name);

                        void *prev_compile_unit = current_compile_unit;
                        if(sym_find_compilation_unit_by_name(dwarfinfo,
                                    &current_compile_unit, name, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        if(current_compile_unit)
                            printf("Selected compile unit '%s'\n", name);
                        else{
                            current_compile_unit = prev_compile_unit;
                            printf("Compile unit '%s' not found\n", name);
                        }

                        printf("\n");

                        break;
                    }
                case CHOICE_LIST_COMPILE_UNITS:
                    {
                        if(sym_display_compilation_units(dwarfinfo, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }
                        printf("\n");
                        break;
                    }
                case CHOICE_RESET_CURRENT_COMPILE_UNIT:
                    {
                        current_compile_unit = NULL;
                        printf("Current compile unit reset\n\n");
                        break;
                    }
                case CHOICE_DISPLAY_DIE_TREE_FROM_ROOT:
                    {
                        if(!current_compile_unit){
                            printf("No selected compile unit\n\n");
                            break;
                        }

                        void *comp_root_die = NULL;

                        if(sym_get_compilation_unit_root_die(current_compile_unit,
                                    &comp_root_die, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        sym_display_die_tree_starting_from(comp_root_die);
                        printf("\n");
                        break;
                    }
                case CHOICE_CONVERT_LINE_NUMBER_TO_PC:
                    {
                        if(!current_compile_unit){
                            printf("No selected compile unit\n\n");
                            break;
                        }

                        uint64_t lineno = 0;
                        printf("\nLine number? ");
                        scanf("%lld", &lineno);

                        uint64_t pc = 0;

                        if(sym_lineno_to_pc_b(dwarfinfo, current_compile_unit,
                                    &lineno, &pc, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        void *comp_root_die = NULL;

                        if(sym_get_compilation_unit_root_die(current_compile_unit,
                                    &comp_root_die, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        char *name = NULL;
                        if(sym_get_die_name(comp_root_die, &name, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        printf("\n%s:%lld: %#llx\n\n", name, lineno, pc);

                        break;
                    }
                case CHOICE_CONVERT_PC_TO_LINE_NUMBER:
                    {
                        if(!current_compile_unit){
                            printf("No selected compile unit\n\n");
                            break;
                        }

                        uint64_t pc = 0;
                        printf("\nEnter PC: ");
                        scanf("%llx", &pc);

                        uint64_t lineno = 0;
                        if(sym_pc_to_lineno_b(dwarfinfo, current_compile_unit,
                                    pc, &lineno, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        printf("%#llx = line %lld\n\n", pc, lineno);

                        break;
                    }
                case CHOICE_GET_PC_VALUES_FROM_LINE_NUMBER:
                    {
                        if(!current_compile_unit){
                            printf("No selected compile unit\n\n");
                            break;
                        }

                        uint64_t lineno = 0;
                        printf("\nEnter line number: ");
                        scanf("%lld", &lineno);

                        uint64_t *pcs = NULL;
                        int len = 0;
                        if(sym_get_pc_values_from_lineno(dwarfinfo,
                                    current_compile_unit, lineno, &pcs,
                                    &len, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        if(len == 0){
                            printf("No PC values for source line %lld\n\n",
                                    lineno);
                            break;
                        }

                        putchar('\n');

                        for(int i=0; i<len-1; i++)
                            printf("%#llx, ", pcs[i]);

                        if(len > 0)
                            printf("%#llx\n\n", pcs[len - 1]);

                        free(pcs);

                        break;
                    }
                case CHOICE_GET_LINE_INFO_FROM_PC:
                    {
                        // XXX this option doesn't need a compile unit to function,
                        // client supplies the program counter and it is
                        // found that way
                        uint64_t pc = 0;
                        printf("\nEnter PC: ");
                        scanf("%llx", &pc);

                        char *srcfilename = NULL, *srcfunction = NULL;
                        uint64_t srcfilelineno = 0;

                        void *root_die = NULL;
                        
                        if(sym_get_line_info_from_pc(dwarfinfo, pc,
                                    &srcfilename, &srcfunction,
                                    &srcfilelineno, &root_die, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        if(root_die){
                            printf("%#llx: %s:%s:%lld\n",
                                    pc, srcfilename, srcfunction, srcfilelineno);

                            free(srcfilename);
                            free(srcfunction);
                        }
                        else{
                            printf("\nCouldn't get line info\n");
                        }

                        putchar('\n');

                        break;
                    }
                case CHOICE_GET_LINE_AFTER_PC:
                    {
                        uint64_t pc = 0;
                        printf("\nEnter current PC: ");
                        scanf("%llx", &pc);

                        uint64_t next_line_pc = 0;

                        void *root_die = NULL;

                        if(sym_get_pc_of_next_line(dwarfinfo, pc,
                                    &next_line_pc, &root_die, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        if(!root_die){
                            printf("No line after PC %#llx\n\n", pc);
                            break;
                        }

                        printf("Next line is at %#llx\n\n", next_line_pc);

                        char *srcfilename = NULL, *srcfunction = NULL;
                        uint64_t srcfilelineno = 0;

                        if(sym_get_line_info_from_pc(dwarfinfo, pc,
                                    &srcfilename, &srcfunction,
                                    &srcfilelineno, &root_die, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        if(root_die){
                            printf("Next line is at %#llx: %s:%s:%lld\n",
                                    next_line_pc, srcfilename, srcfunction, srcfilelineno);

                            free(srcfilename);
                            free(srcfunction);
                        }
                        else{
                            printf("\nCouldn't get line info\n");
                        }

                        putchar('\n');

                        break;
                    }
                case CHOICE_GET_VARIABLE_DIES_AROUND_PC:
                    {
                        uint64_t pc = 0;
                        printf("\nEnter PC: ");
                        scanf("%llx", &pc);

                        void **vardies = NULL;
                        int len = 0;

                        if(sym_get_variable_dies(dwarfinfo, pc,
                                    &vardies, &len, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        if(len == 0){
                            printf("No variable DIEs around PC %#llx\n\n", pc);
                            break;
                        }

                        for(int i=0; i<len; i++)
                            sym_display_die(vardies[i]);

                        free(vardies);

                        break;
                    }
                case CHOICE_DISPLAY_DIE_MENU:
                    {
                        if(!current_compile_unit){
                            printf("No selected compile unit\n\n");
                            break;
                        }

                        display_compile_unit_menu = 0;
                        printf("\n");
                        break;
                    }
                case CHOICE_QUIT:
                    {
                        sym_end(&dwarfinfo);
                        return 0;
                    }
                default:
                    {
                        printf("Invalid choice %d\n", choice);
                        break;
                    }
            };
        }
        else{
            printf("1. Find DIE in this compile unit by name\n"
                    "2. Find a function DIE via an arbitrary PC\n"
                    "3. Display DIE tree of current DIE\n"
                    "4. Display parameters of current DIE\n"
                    "5. Get struct or union members of current DIE\n"
                    "6. Reset current DIE\n"
                    "7. Display compilation unit menu\n");

            int choice = 0;
            scanf("%d", &choice);

            switch(choice){
                case CHOICE_FIND_DIE_BY_NAME:
                    {
                        void *comp_root_die = NULL;

                        if(sym_get_compilation_unit_root_die(current_compile_unit,
                                    &comp_root_die, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        sym_display_die_tree_starting_from(comp_root_die);

                        printf("\nWhich? Enter name: ");
                        char name[200] = {0};
                        scanf("%s", name);

                        void *prev_die = current_die;
                        if(sym_find_die_by_name(current_compile_unit, name,
                                    &current_die, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        if(current_die)
                            printf("Selected die '%s'\n", name);
                        else{
                            current_die = prev_die;
                            printf("DIE '%s' not found\n", name);
                        }

                        printf("\n");
                        break;
                    }
                case CHOICE_FIND_FUNCTION_DIE_BY_PC:
                    {
                        void *d = current_die;
                        
                        if(!current_die){
                            if(sym_get_compilation_unit_root_die(current_compile_unit,
                                        &d, &sym_error)){
                                printf("error: %s\n", sym_strerror(sym_error));
                                errclear(&sym_error);
                                break;
                            }
                        }
                        
                        sym_display_die_tree_starting_from(d);

                        uint64_t pc = 0;
                        printf("\nEnter PC: ");
                        scanf("%llx", &pc);

                        void *prev_die = current_die;
                        if(sym_find_function_die_by_pc(current_compile_unit,
                                    pc, &current_die, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        if(current_die){
                            char *name = NULL;
                            if(sym_get_die_name(current_die, &name, &sym_error)){
                                printf("error: %s\n", sym_strerror(sym_error));
                                errclear(&sym_error);
                                break;
                            }

                            printf("Selected die '%s'\n", name);
                        }
                        else{
                            current_die = prev_die;
                            printf("Function DIE with PC %#llx not found\n", pc);
                        }

                        printf("\n");
                        break;
                    }
                case CHOICE_DISPLAY_CURRENT_DIE:
                    {
                        if(!current_die){
                            printf("No DIE selected\n\n");
                            break;
                        }

                        sym_display_die_tree_starting_from(current_die);
                        printf("\n");
                        break;
                    }
                case CHOICE_GET_PARAMETERS_FROM_DIE:
                    {
                        if(!current_die){
                            printf("No DIE selected\n\n");
                            break;
                        }

                        putchar('\n');

                        int len = 0;
                        void **params = NULL;
                        if(sym_get_function_die_parameters(current_die,
                                    &params, &len, &sym_error)){
                            printf("error: %s\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        if(!params)
                            printf("Couldn't get parameters from the current DIE\n");
                        else{
                            for(int i=0; i<len; i++)
                                sym_display_die(params[i]);

                            free(params);
                        }

                        printf("\n%d parameters\n\n", len);

                        break;
                    }
                case CHOICE_GET_STRUCT_OR_UNION_MEMBERS_FROM_DIE:
                    {
                        if(!current_die){
                            printf("No DIE selected\n\n");
                            break;
                        }

                        int len = 0;
                        void **members = NULL;
                        if(sym_get_die_members(current_die, &members, &len,
                                    &sym_error)){
                            printf("error: %s\n\n", sym_strerror(sym_error));
                            errclear(&sym_error);
                            break;
                        }

                        for(int i=0; i<len; i++)
                            sym_display_die(members[i]);

                        free(members);

                        putchar('\n');

                        break;
                    }
                case CHOICE_RESET_CURRENT_DIE:
                    {
                        current_die = NULL;
                        printf("Current DIE reset\n\n");
                        break;
                    }
                case CHOICE_DISPLAY_COMPILATION_UNIT_MENU:
                    {
                        display_compile_unit_menu = 1;
                        printf("\n");
                        break;
                    }
                default:
                    {
                        printf("Invalid choice %d\n", choice);
                        break;
                    }
            };
        }
    }

    return 0;
}
