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
        CHOICE_DISPLAY_DIE_MENU
    };

    void *current_die = NULL;

    enum {
        CHOICE_FIND_DIE_BY_NAME = 1,
        CHOICE_FIND_FUNCTION_DIE_BY_PC,
        CHOICE_DISPLAY_CURRENT_DIE,
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
                    "5. Display DIE menu\n");
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
                    "4. Reset current DIE\n"
                    "5. Display compilation unit menu\n");

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
