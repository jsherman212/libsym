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

    int display_compile_unit_menu = 1;

    void *current_compile_unit = NULL;

    enum {
        CHOICE_CHOOSE_COMPILE_UNIT = 1,
        CHOICE_LIST_COMPILE_UNITS,
        CHOICE_RESET_CURRENT_COMPILE_UNIT,
        CHOICE_DISPLAY_DIE_TREE_FROM_ROOT,
        CHOICE_CONVERT_LINE_NUMBER_TO_PC,
        CHOICE_GET_LINE_INFO_FROM_PC,
        CHOICE_DISPLAY_DIE_MENU
    };

    void *current_die = NULL;

    enum {
        CHOICE_FIND_DIE_BY_NAME = 1,
        CHOICE_FIND_FUNCTION_DIE_BY_PC,
        CHOICE_DISPLAY_CURRENT_DIE,
        CHOICE_GET_PARAMETERS_FROM_DIE,
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
                    "6. Get line info from an arbitrary PC\n"
                    "7. Display DIE menu\n");
            int choice = 0;
            scanf("%d", &choice);

            switch(choice){
                case CHOICE_CHOOSE_COMPILE_UNIT:
                    {
                        printf("Compile units:\n");
                        sym_display_compilation_units(dwarfinfo);
                        printf("\nWhich? Enter name: ");
                        char name[200] = {0};
                        scanf("%s", name);

                        void *prev_compile_unit = current_compile_unit;
                        current_compile_unit =
                            sym_find_compilation_unit_by_name(dwarfinfo, name);

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
                        sym_display_compilation_units(dwarfinfo);
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

                        void *comp_root_die =
                            sym_get_compilation_unit_root_die(current_compile_unit);

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

                        uint64_t pc =
                            sym_lineno_to_pc_b(current_compile_unit, &lineno);

                        void *comp_root_die =
                            sym_get_compilation_unit_root_die(current_compile_unit);
                        char *name = sym_get_die_name(comp_root_die);

                        printf("\n%s:%lld: %#llx\n\n", name, lineno, pc);

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

                        void *root_die =
                            sym_get_line_info_from_pc(dwarfinfo, pc,
                                    &srcfilename, &srcfunction, &srcfilelineno);

                        if(root_die){
                            printf("%#llx: %s:%s:%lld\n",
                                    pc, srcfilename, srcfunction, srcfilelineno);
                        }
                        else{
                            printf("\nCouldn't get line info\n");
                        }

                        putchar('\n');

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
                    "5. Reset current DIE\n"
                    "6. Display compilation unit menu\n");

            int choice = 0;
            scanf("%d", &choice);

            switch(choice){
                case CHOICE_FIND_DIE_BY_NAME:
                    {
                        void *comp_root_die =
                            sym_get_compilation_unit_root_die(current_compile_unit);

                        sym_display_die_tree_starting_from(comp_root_die);

                        printf("\nWhich? Enter name: ");
                        char name[200] = {0};
                        scanf("%s", name);

                        void *prev_die = current_die;
                        current_die = sym_find_die_by_name(current_compile_unit, name);

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
                        
                        if(!current_die)
                            d = sym_get_compilation_unit_root_die(current_compile_unit);
                        
                        sym_display_die_tree_starting_from(d);

                        uint64_t pc = 0;
                        printf("\nEnter PC: ");
                        scanf("%llx", &pc);

                        void *prev_die = current_die;
                        current_die = sym_find_function_die_by_pc(current_compile_unit, pc);

                        if(current_die){
                            char *name = sym_get_die_name(current_die);
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
                        void **params =
                            sym_get_function_die_parameters(current_die, &len);

                        if(!params)
                            printf("Couldn't get parameters from the current DIE\n");
                        else{
                            int idx = 0;
                            void *curparam = params[idx];

                            while(curparam){
                                sym_display_die(curparam);
                                curparam = params[++idx];
                            }
                        }

                        printf("\n%d parameters\n\n", len);

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

    sym_end(&dwarfinfo);

    return 0;
}
