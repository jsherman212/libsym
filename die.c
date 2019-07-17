#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dwarf.h>
#include <libdwarf.h>

#include "common.h"

typedef struct die die_t;

struct die {
    Dwarf_Die die_dwarfdie;
    Dwarf_Unsigned die_dieoffset;

    Dwarf_Half die_tag;
    char *die_tagname;

    Dwarf_Attribute *die_attrs;
    Dwarf_Signed die_attrcnt;

    char *die_diename;

    Dwarf_Half die_haschildren;

    /* non-NULL when this die is a parent */
    /* NULL terminated array of children */
    die_t **die_children;
    int die_numchildren;

    /* non-NULL when this die is a child */
    die_t *die_parent;

    /* If this DIE describes any sort of variable/parameter in the
     * debugged program, the following four are initialized.
     */
    Dwarf_Unsigned die_datatypedieoffset;
    Dwarf_Die die_datatypedie;
    Dwarf_Unsigned die_databytesize;
    char *die_datatypename;

    /* If this DIE represents an inlined subroutine, the following
     * two are initialized.
     */
    int die_inlinedsub;
    Dwarf_Unsigned die_aboriginoff;
};

static inline void write_tabs(int cnt){
    for(int i=0; i<cnt; i++)
        putchar('\t');
}

static int lex_block_count = 0, anon_struct_count = 0, anon_union_count = 0,
           anon_enum_count = 0;

static int is_anonymous_type(die_t *die){
    return (die->die_tag == DW_TAG_structure_type ||
            die->die_tag == DW_TAG_union_type ||
            die->die_tag == DW_TAG_enumeration_type) &&
        !die->die_diename;
}

static int is_inlined_subroutine(die_t *die){
    return die->die_tag == DW_TAG_inlined_subroutine;
}

static int copy_die_info(dwarfinfo_t *dwarfinfo, die_t **die){
    Dwarf_Error d_error;

    int ret = dwarf_diename((*die)->die_dwarfdie, &((*die)->die_diename),
            &d_error);

    ret = dwarf_dieoffset((*die)->die_dwarfdie, &((*die)->die_dieoffset),
            &d_error);

    /*
       if(ret){
       printf("dwarf_diename: error: %s\n", dwarf_errmsg_by_number(ret));
       }
       */

    //dprintf("ret %d\n", ret);
    /*if(ret)
      return ret;
      */
    ret = dwarf_tag((*die)->die_dwarfdie, &((*die)->die_tag), &d_error);

    // XXX: inside iosdbg get rid of this auto naming so I can test
    // for NULL names for anonymous types
    if(is_anonymous_type(*die)){
        const char *type = "STRUCT";
        int *cnter = &anon_struct_count;

        if((*die)->die_tag == DW_TAG_union_type){
            type = "UNION";
            cnter = &anon_union_count;
        }
        else if((*die)->die_tag == DW_TAG_enumeration_type){
            type = "ENUM";
            cnter = &anon_enum_count;
        }

        asprintf(&((*die)->die_diename), "ANON_%s_%d", type, (*cnter)++);
    }
    else if(is_inlined_subroutine(*die)){
        (*die)->die_inlinedsub = 1;

        Dwarf_Attribute typeattr;
        ret = dwarf_attr((*die)->die_dwarfdie, DW_AT_abstract_origin,
                &typeattr, &d_error);

        if(ret == DW_DLV_OK){
            dwarf_global_formref(typeattr, &((*die)->die_aboriginoff),
                    &d_error);
        }
    }

    //dprintf("ret %d\n", ret);
    /*if(ret)
      return ret;
      */
    ret = dwarf_get_TAG_name((*die)->die_tag,
            (const char **)&((*die)->die_tagname));

    /* Label these ourselves */
    if(!(*die)->die_diename){
        if((*die)->die_tag == DW_TAG_lexical_block){
            asprintf(&((*die)->die_diename),
                    "LEXICAL_BLOCK_%d",// (auto-named by libsym)",
                    lex_block_count++);
        }
    }

    //dprintf("ret %d\n", ret);
    /*if(ret)
      return ret;
      */
    ret = dwarf_attrlist((*die)->die_dwarfdie, &((*die)->die_attrs),
            &((*die)->die_attrcnt), &d_error);

    //dprintf("ret %d\n", ret);
    /*if(ret)
      return ret;
      */
    ret = dwarf_die_abbrev_children_flag((*die)->die_dwarfdie,
            &((*die)->die_haschildren));


    Dwarf_Attribute attr;
    ret = dwarf_attr((*die)->die_dwarfdie, DW_AT_type, &attr, &d_error);

    if(ret == DW_DLV_OK){
        dwarf_global_formref(attr, &((*die)->die_datatypedieoffset),
                &d_error);
        ret = dwarf_offdie(dwarfinfo->di_dbg, (*die)->die_datatypedieoffset,
                &((*die)->die_datatypedie), &d_error);

        if(ret == DW_DLV_OK){
            dwarf_diename((*die)->die_datatypedie, &((*die)->die_datatypename),
                    &d_error);
        }

        ret = dwarf_bytesize((*die)->die_datatypedie, &((*die)->die_databytesize),
                    &d_error);

        /*
        ret = dwarf_attr((*die)->die_datatypedie, DW_AT_byte_size, &attr,
                &d_error);

        //ret = dwarf_errno(d_error);
        printf("dwarf_attr: %s (%#x)\n", dwarf_errmsg_by_number(ret), ret);
        //if(ret){
            //printf("dwarf_attr: %s (%#x)\n", dwarf_errmsg_by_number(ret), ret);
        //}

        if(ret == DW_DLV_OK){
            ret = dwarf_global_formref(attr, &((*die)->die_databytesize), &d_error);
            ret = dwarf_errno(d_error);
            printf("dwarf_global_formref: %s (%#x)\n", dwarf_errmsg_by_number(ret), ret);
        }
        */
    }

    //dprintf("ret %d\n", ret);
    /*if(ret)
      return ret;
      */
    return ret;
}

static Dwarf_Half die_has_children(Dwarf_Die d){
    Dwarf_Half result = 0;
    dwarf_die_abbrev_children_flag(d, &result);

    return result;
}

static char *get_die_data_type(die_t *die){
    Dwarf_Error d_error = NULL;

    char *name = NULL;
    dwarf_diename(die->die_datatypedie, &name, &d_error);
    /*Dwarf_Unsigned ret = dwarf_errno(d_error);

    if(ret){
        printf("dwarf_diename: %s (%#llx)\n", dwarf_errmsg_by_number(ret), ret);

    }*/

    return name;
}

#define GREEN "\033[32m"
#define RED "\033[31m"
#define CYAN "\033[36m"
#define MAGENTA "\033[35m"
#define LIGHT_GREEN "\033[93m"
#define LIGHT_BLUE "\033[94m" // XXX purple?
#define RESET "\033[39m"

static int cnt = 1;
static void describe_die(die_t *die, int level){
    if(!die)
        return;

    printf("%#llx: <%d> <%s>: '%s%s%s', is parent: %d, type DIE at %s%#llx%s",
            die->die_dieoffset, level, die->die_tagname,
            die->die_haschildren?GREEN:"", die->die_diename, die->die_haschildren?RESET:"",
            die->die_haschildren, die->die_datatypedieoffset!=0?CYAN:"",
            die->die_datatypedieoffset, die->die_datatypedieoffset!=0?RESET:"");
    if(level == 1)
        cnt++;

    if(die->die_datatypedieoffset!=0){
        //char *type = get_die_data_type(die);
        printf(", type = '"LIGHT_BLUE"%s"RESET"',"
                " sizeof('"LIGHT_BLUE"%s"RESET"') = "LIGHT_GREEN"%#llx"RESET"",
                die->die_datatypename, die->die_datatypename, die->die_databytesize);
    }

    // XXX later, abstract origin is '%s'
    if(die->die_inlinedsub)
        printf(", abstract origin %s%#llx%s", MAGENTA, die->die_aboriginoff, RESET);

    if(!die->die_haschildren && die->die_parent){
        printf(", parent DIE name '"GREEN"%s"RESET"'\n", die->die_parent->die_diename);
    }
    else if(!die->die_haschildren && !die->die_parent){
        printf(", "RED"no parent???"RESET"\n");
    }
    else if(die->die_haschildren && die->die_parent){
        printf(", parent DIE name '"GREEN"%s"RESET"'\n", die->die_parent->die_diename);
    }
    else{
        putchar('\n');
    }
}

static die_t *create_new_die(dwarfinfo_t *dwarfinfo, Dwarf_Die based_on){
    // XXX do I don't have to waste time setting stuff to NULL
    die_t *d = calloc(1, sizeof(die_t));
    d->die_dwarfdie = based_on;

    // XXX some of the stuff in this function will fail,
    // it is fine
    copy_die_info(dwarfinfo, &d);

    if(d->die_haschildren){
        d->die_children = malloc(sizeof(die_t));
        d->die_children[0] = NULL;

        d->die_numchildren = 0;
    }

    return d;
}

static int add_die_to_tree(die_t *die){
    const Dwarf_Half accepted_tags[] = {
        DW_TAG_compile_unit, DW_TAG_subprogram, DW_TAG_inlined_subroutine,
        DW_TAG_formal_parameter, DW_TAG_enumeration_type, DW_TAG_enumerator,
        DW_TAG_structure_type, DW_TAG_union_type, DW_TAG_member,
        DW_TAG_variable, DW_TAG_lexical_block
    };

    size_t count = sizeof(accepted_tags) / sizeof(Dwarf_Half);

    for(size_t i=0; i<count; i++){
        if(die->die_tag == accepted_tags[i])
            return 1;
    }

    return 0;
}

static die_t *CUR_PARENTS[1000] = {0};

/* credit is due: simplereader.c */

/* This tree only contains DIEs with these tags:
 *      DW_TAG_compile_unit
 *      DW_TAG_subprogram
 *      DW_TAG_inlined_subroutine
 *      DW_TAG_formal_parameter
 *      DW_TAG_enumeration_type / DW_TAG_enumerator
 *      DW_TAG_structure_type
 *      DW_TAG_union_type
 *      DW_TAG_member
 *      DW_TAG_variable
 *      DW_TAG_lexical_block
 *
 * It becomes too much to keep track off every possible
 * aspect of a DIE, and we're able to retrieve the info we need if
 * we already have a target DIE.
 */
static void construct_die_tree(dwarfinfo_t *dwarfinfo, die_t *root,
        die_t *parent, die_t *current, int level){
    int is_info = 1;
    Dwarf_Die child_die, cur_die = current->die_dwarfdie;
    Dwarf_Error d_error;

    int ret = DW_DLV_OK;

    // XXX function later
    if(add_die_to_tree(current)){
        if(current->die_haschildren){
            /* Keep track of the parent DIEs we visit. */
            CUR_PARENTS[level] = current;

            if(level > 0){
                die_t *parent = CUR_PARENTS[level - 1];

                if(parent){
                    die_t **children = realloc(parent->die_children,
                            (++parent->die_numchildren) * sizeof(die_t));
                    parent->die_children = children;
                    parent->die_children[parent->die_numchildren - 1] = current;
                    parent->die_children[parent->die_numchildren] = NULL;

                    current->die_parent = parent;
                }
            }
        }
        else{
            if(level > 0){
                int sub = 1;
                die_t *parent = CUR_PARENTS[level - sub];

                /* Find the closest valid parent. We could be multiple levels
                 * deep without seeing `level` amount of parent DIEs.
                 */
                while(!parent)
                    parent = CUR_PARENTS[level - (++sub)];

                if(parent){
                    die_t **children = realloc(parent->die_children,
                            (++parent->die_numchildren) * sizeof(die_t));
                    parent->die_children = children;
                    parent->die_children[parent->die_numchildren - 1] = current;
                    parent->die_children[parent->die_numchildren] = NULL;

                    current->die_parent = parent;
                }
            }
        }

        write_tabs(level);
        describe_die(current, level);
    }

    for(;;){
        ret = dwarf_child(cur_die, &child_die, NULL);

        if(ret == DW_DLV_ERROR){
            dprintf("dwarf_child level %d: %s\n", level, dwarf_errmsg_by_number(ret));
            exit(1);
        }
        else if(ret == DW_DLV_OK){
            die_t *cd = create_new_die(dwarfinfo, child_die);
            construct_die_tree(dwarfinfo, root, parent, cd, level+1);
            dwarf_dealloc(dwarfinfo->di_dbg, child_die, DW_DLA_DIE);
        }

        Dwarf_Die sibling_die;
        ret = dwarf_siblingof_b(dwarfinfo->di_dbg, cur_die, is_info,
                &sibling_die, &d_error);

        if(ret == DW_DLV_ERROR){
            dprintf("dwarf_siblingof_b level %d: %s\n", level, dwarf_errmsg_by_number(ret));
            exit(1);
        }
        else if(ret == DW_DLV_NO_ENTRY){
            /* Discard the parent we were on. */
            CUR_PARENTS[level] = NULL;
            return;
        }

        if(cur_die != current->die_dwarfdie)
            dwarf_dealloc(dwarfinfo->di_dbg, cur_die, DW_DLA_DIE);

        cur_die = sibling_die;

        die_t *current = create_new_die(dwarfinfo, cur_die);

        if(add_die_to_tree(current)){
            if(current->die_haschildren){
                CUR_PARENTS[level] = current;

                if(level > 0){
                    die_t *parent = CUR_PARENTS[level - 1];

                    if(parent){
                        die_t **children = realloc(parent->die_children,
                                (++parent->die_numchildren) * sizeof(die_t));
                        parent->die_children = children;
                        parent->die_children[parent->die_numchildren - 1] = current;
                        parent->die_children[parent->die_numchildren] = NULL;

                        current->die_parent = parent;
                    }
                }
            }
            else{
                if(level > 0){
                    int sub = 1;
                    die_t *parent = CUR_PARENTS[level - sub];

                    while(!parent)
                        parent = CUR_PARENTS[level - (++sub)];

                    if(parent){
                        die_t **children = realloc(parent->die_children,
                                (++parent->die_numchildren) * sizeof(die_t));
                        parent->die_children = children;
                        parent->die_children[parent->die_numchildren - 1] = current;
                        parent->die_children[parent->die_numchildren] = NULL;

                        current->die_parent = parent;
                    }
                }
            }

            write_tabs(level);
            describe_die(current, level);
        }
    }
}

/* Chapter 2.3:
 * The ownership relation of debugging information entries is achieved
 * naturally because the debugging information is represented as a tree.
 * The nodes of the tree are the debugging information entries themselves. 
 * The child entries of any node are exactly those debugging information 
 * entries owned by that node.
 * The tree itself is represented by flattening it in prefix order.
 */
int initialize_and_build_die_tree_from_root_die(dwarfinfo_t *dwarfinfo,
        die_t **_root_die, char **error){
    int is_info = 1;
    Dwarf_Error d_error;

    Dwarf_Die cu_rootdie;
    int ret = dwarf_siblingof_b(dwarfinfo->di_dbg, NULL, is_info,
            &cu_rootdie, &d_error);

    if(ret == DW_DLV_ERROR){
        asprintf(error, "dwarf_siblingof_b: %s",
                dwarf_errmsg_by_number(ret));
        return 1;
    }

    die_t *root_die = create_new_die(dwarfinfo, cu_rootdie);
    memset(CUR_PARENTS, 0, sizeof(CUR_PARENTS));
    CUR_PARENTS[0] = root_die;

    //if(strcmp(root_die->die_diename, "source/cmd/documentation.c") != 0)
      //  return 0;
    construct_die_tree(dwarfinfo, root_die, NULL, root_die, 0);

    // XXX XXX second time around, connect die_datatypes

    putchar('\n');
    printf("Children for root DIE '%s':\n", root_die->die_diename);

    die_t **children = root_die->die_children;

    int nonnull = 0;
    for(int i=0; i<root_die->die_numchildren; i++){
        die_t *d = children[i];

        printf("%d: DIE '%s'\n", i, d->die_diename);

        if(d->die_diename)
            nonnull++;
    }

    printf("%d children, and %d nonnull children\n\n",
            root_die->die_numchildren, nonnull);

    *_root_die = root_die;

    return 0;
}

// XXX need two tab level indent for printfs
void display_from_root_die(die_t *root_die){
    //    dprintf("root_die %p\n", root_die);

    printf("\t\troot die <TAG: '%s'>:\n"
            "\t\t\tdie name: '%s'\n",
            root_die->die_tagname, root_die->die_diename);
}
