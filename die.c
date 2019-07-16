#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libdwarf.h>

#include "common.h"

typedef struct die die_t;

struct die {
    Dwarf_Die die_dwarfdie;

    Dwarf_Half die_tag;
    const char *die_tagname;

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
};

static int copy_die_info(die_t **die){
    Dwarf_Error d_error;

    int ret = dwarf_diename((*die)->die_dwarfdie, &((*die)->die_diename),
            &d_error);

    //dprintf("ret %d\n", ret);
    if(ret)
        return ret;

    ret = dwarf_tag((*die)->die_dwarfdie, &((*die)->die_tag), &d_error);

    //dprintf("ret %d\n", ret);
    if(ret)
        return ret;

    ret = dwarf_get_TAG_name((*die)->die_tag, &((*die)->die_tagname));

    //dprintf("ret %d\n", ret);
    if(ret)
        return ret;

    ret = dwarf_attrlist((*die)->die_dwarfdie, &((*die)->die_attrs),
            &((*die)->die_attrcnt), &d_error);

    //dprintf("ret %d\n", ret);
    if(ret)
        return ret;

    ret = dwarf_die_abbrev_children_flag((*die)->die_dwarfdie,
            &((*die)->die_haschildren));

    //dprintf("ret %d\n", ret);
    if(ret)
        return ret;

    return 0;
}

static Dwarf_Half die_has_children(Dwarf_Die d){
    Dwarf_Half result = 0;
    dwarf_die_abbrev_children_flag(d, &result);

    return result;
}

#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[39m"

static int cnt = 1;
static void describe_die(die_t *die, int level){
    if(!die)
        return;

    printf("<%d> <%s>: '%s%s%s', is parent: %d", level, die->die_tagname,
            die->die_haschildren?GREEN:"", die->die_diename, die->die_haschildren?RESET:"",
            die->die_haschildren);
    if(level == 1)
        cnt++;

    if(!die->die_haschildren && die->die_parent){
        printf(", parent DIE name '"GREEN"%s"RESET"'\n", die->die_parent->die_diename);
    }
    else if(!die->die_haschildren && !die->die_parent){
        printf(", no parent???\n");
    }
    else if(die->die_haschildren && die->die_parent){
        printf(", %d children, parent DIE name '"GREEN"%s"RESET"'\n", die->die_numchildren,
                die->die_parent->die_diename);
    }
    else{
        putchar('\n');
    }
}

static die_t *create_new_die(Dwarf_Die based_on){
    // XXX do I don't have to waste time setting stuff to NULL
    die_t *d = calloc(1, sizeof(die_t));
    d->die_dwarfdie = based_on;

    if(copy_die_info(&d))
        return NULL;

    if(d->die_haschildren){
        d->die_children = malloc(sizeof(die_t));
        d->die_children[0] = NULL;

        d->die_numchildren = 0;
    }

    return d;
}

static inline void write_tabs(int cnt){
    for(int i=0; i<cnt; i++)
        putchar('\t');
}

//static die_t *CUR_PARENT = NULL;
static die_t *CUR_PARENTS[200] = {0};
//static int PARENT_IDX = 0;
//static int SUB_FROM_LEVEL = 0;

/* credit is due: simplereader.c */
static void construct_die_tree(dwarfinfo_t *dwarfinfo, die_t *root,
        die_t *parent, die_t *current, int level){
    if(!current)
        return;

    int is_info = 1;
    Dwarf_Die child_die, cur_die = current->die_dwarfdie;
    Dwarf_Error d_error;

    int ret = DW_DLV_OK;

    /* Keep track of the parent DIEs we visit. */
    if(current->die_haschildren)
        CUR_PARENTS[level] = current;

    //write_tabs(level);
    if(current->die_haschildren){
        /*
        printf("this die '%s' has children, setting CUR_PARENT\n",
                current->die_diename);
                */
        //printf("*****this die '%s' has children and its parent is '%s'\n",
          //      current->die_diename, CUR_PARENTS[0]->die_diename);
        if(current != root){
            // XXX ... this can be assumed?
            die_t *parent = CUR_PARENTS[0];
            die_t **children = realloc(parent->die_children,
                    (++parent->die_numchildren) * sizeof(die_t));
            parent->die_children = children;
            parent->die_children[parent->die_numchildren - 1] = current;
            parent->die_children[parent->die_numchildren] = NULL;

            current->die_parent = parent;
        }
        //CUR_PARENTS[level] = current;
        //CUR_PARENTS[PARENT_IDX++] = current;
        //CUR_PARENT = current;
    }
    else{
        /*
        printf("this die '%s' doesn't have children, CUR_PARENT is '%s',"
                " adding child die\n",
                current->die_diename, CUR_PARENT->die_diename);
        */

        /* previous parent */
        int sub = 1;
        die_t *parent = CUR_PARENTS[level - sub];

        /*
        write_tabs(level);
        if(parent){
            printf("\033[32mparent for DIE '%s' is '%s'\033[39m\n",
                    current->die_diename, parent->die_diename);
        }
        else{
            printf("\033[31mparent is NULL, searching backwards in CUR_PARENTS\033[39m\n");
        }
        */

        /* find the closest valid parent */
        while(!parent)
            parent = CUR_PARENTS[level - (++sub)];
   
        if(parent){
            die_t **children = realloc(parent->die_children,
                    (++parent->die_numchildren) * sizeof(die_t));
            parent->die_children = children;
            parent->die_children[parent->die_numchildren - 1] = current;
            parent->die_children[parent->die_numchildren] = NULL;

            current->die_parent = parent;

            /*
            write_tabs(level);
            printf("current DIE parent's name '%s' actual DIE parent name '%s'\n",
                    current->die_parent->die_diename, parent->die_diename);
                    */
        }
    }

    if(current){
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
            die_t *cd = create_new_die(child_die);
            construct_die_tree(dwarfinfo, root, parent, cd, level+1);
            dwarf_dealloc(dwarfinfo->di_dbg, child_die, DW_DLA_DIE);
        }

        //write_tabs(level);
        //dprintf("sibling die\n");
        Dwarf_Die sibling_die;
        ret = dwarf_siblingof_b(dwarfinfo->di_dbg, cur_die, is_info,
                &sibling_die, &d_error);

        if(ret == DW_DLV_ERROR){
            dprintf("dwarf_siblingof_b level %d: %s\n", level, dwarf_errmsg_by_number(ret));
            exit(1);
        }
        else if(ret == DW_DLV_NO_ENTRY){
            //write_tabs(level);
            //dprintf("done at this level, breaking\n");

            /* Discard the parent we were on. */
            // XXX XXX remove comment to fix bug
            CUR_PARENTS[level] = NULL;
            break;
        }

        if(cur_die != current->die_dwarfdie)
            dwarf_dealloc(dwarfinfo->di_dbg, cur_die, DW_DLA_DIE);

        cur_die = sibling_die;

        die_t *current = create_new_die(cur_die);

        //printf("current %p\n", current);
        if(current && current->die_haschildren)
            CUR_PARENTS[level] = current;

        //write_tabs(level);
        if(current && current->die_haschildren){
           /* 
            printf("*****this die '%s' has children, setting CUR_PARENT\n",
                    current->die_diename);
                    */

            //printf("*****this die '%s' has children and its parent is '%s'\n",
              //      current->die_diename, CUR_PARENTS[0]->die_diename);
            
            die_t *parent = CUR_PARENTS[0];
            die_t **children = realloc(parent->die_children,
                    (++parent->die_numchildren) * sizeof(die_t));
            parent->die_children = children;
            parent->die_children[parent->die_numchildren - 1] = current;
            parent->die_children[parent->die_numchildren] = NULL;

            current->die_parent = parent;
        }
        else if(current && !current->die_haschildren){
            /*
            printf("this die '%s' doesn't have children, CUR_PARENT is '%s',"
                    " adding child die\n",
                    current->die_diename, CUR_PARENT->die_diename);
            */
            int sub = 1;
            die_t *parent = CUR_PARENTS[level - sub];
            /*
            write_tabs(level);
            if(parent){
                printf("\033[32mparent for DIE '%s' is '%s'\033[39m\n",
                        current->die_diename, parent->die_diename);
            }
            else{
                printf("\033[31mparent is NULL, searching backwards in CUR_PARENTS\033[39m\n");
            }
            */

            while(!parent)
                parent = CUR_PARENTS[level - (++sub)];
            //die_t *parent = CUR_PARENTS[PARENT_IDX - 1];
            //CUR_PARENT = CUR_PARENTS[level - 1];

            if(parent){
                die_t **children = realloc(parent->die_children,
                        (++parent->die_numchildren) * sizeof(die_t));
                parent->die_children = children;
                parent->die_children[parent->die_numchildren - 1] = current;
                parent->die_children[parent->die_numchildren] = NULL;

                //current->die_parent = CUR_PARENT;
                //current->die_parent = CUR_PARENTS[level - 1];
                current->die_parent = parent;
                /*
                write_tabs(level);
                printf("current DIE parent's name '%s' actual DIE parent name '%s'\n",
                        current->die_parent->die_diename, parent->die_diename);
                        */
            }
        }

        if(current){
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

    die_t *root_die = create_new_die(cu_rootdie);
    memset(CUR_PARENTS, 0, sizeof(CUR_PARENTS));
    CUR_PARENTS[0] = root_die;

    //if(strcmp(root_die->die_diename, "source/dbgops.c") != 0)
    //    return 0;
    construct_die_tree(dwarfinfo, root_die, NULL, root_die, 0);
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
