#include <stdio.h>
#include <stdlib.h>

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

static void describe_die(die_t *die){
    if(!die)
        return;

    printf("<%s>: '%s', parent: %d", die->die_tagname,
            die->die_diename, die->die_haschildren);

    if(!die->die_haschildren && die->die_parent){
        printf(", parent name '%s'\n", die->die_parent->die_diename);
    }
    else if(!die->die_haschildren && !die->die_parent){
        printf(", no parent???\n");
    }
    else if(die->die_haschildren){
        printf(", %d children\n", die->die_numchildren);
    }
    else{
        putchar('\n');
    }

    /*
    if(die->die_haschildren && die->die_parent){
        printf(", parent name '%s'\n",
                die->die_parent->die_diename?die->die_parent->die_diename:"NULL??");
    }
    else{
        putchar('\n');
    }
    */
}

static die_t *create_new_die(Dwarf_Die based_on){
    // XXX do I don't have to waste time setting stuff to NULL
    die_t *d = calloc(1, sizeof(die_t));
    d->die_dwarfdie = based_on;

    copy_die_info(&d);

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

static die_t *CUR_PARENT = NULL;

/* credit is due: simplereader.c */
static void construct_die_tree(dwarfinfo_t *dwarfinfo, die_t *current,
        int level){
    int is_info = 1;
    Dwarf_Die child_die, cur_die = current->die_dwarfdie;
    Dwarf_Error d_error;

    int ret = DW_DLV_OK;

    //write_tabs(level);
    if(current->die_haschildren){
        /*
        printf("this die '%s' has children, setting CUR_PARENT\n",
                current->die_diename);
                */
        CUR_PARENT = current;
    }
    else{
        /*
        printf("this die '%s' doesn't have children, CUR_PARENT is '%s',"
                " adding child die\n",
                current->die_diename, CUR_PARENT->die_diename);
        */
        die_t **children = realloc(CUR_PARENT->die_children,
                (++CUR_PARENT->die_numchildren) * sizeof(die_t));
        CUR_PARENT->die_children = children;
        CUR_PARENT->die_children[CUR_PARENT->die_numchildren - 1] = current;
        CUR_PARENT->die_children[CUR_PARENT->die_numchildren] = NULL;

        current->die_parent = CUR_PARENT;
    }

    if(current->die_diename){
        write_tabs(level);
        describe_die(current);
    }

    for(;;){
        ret = dwarf_child(cur_die, &child_die, NULL);

        if(ret == DW_DLV_ERROR){
            dprintf("dwarf_child level %d: %s\n", level, dwarf_errmsg_by_number(ret));
            exit(1);
        }
        else if(ret == DW_DLV_OK){
            die_t *cd = create_new_die(child_die);
            construct_die_tree(dwarfinfo, cd, level+1);
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
            break;
        }

        if(cur_die != current->die_dwarfdie){
            dwarf_dealloc(dwarfinfo->di_dbg, cur_die, DW_DLA_DIE);
        }

        cur_die = sibling_die;

        die_t *current = create_new_die(cur_die);

        //write_tabs(level);
        if(current->die_haschildren){
            /*
            printf("this die '%s' has children, setting CUR_PARENT\n",
                    current->die_diename);
                    */
            CUR_PARENT = current;
        }
        else{
            /*
            printf("this die '%s' doesn't have children, CUR_PARENT is '%s',"
                    " adding child die\n",
                    current->die_diename, CUR_PARENT->die_diename);
            */
            die_t **children = realloc(CUR_PARENT->die_children,
                    (++CUR_PARENT->die_numchildren) * sizeof(die_t));
            CUR_PARENT->die_children = children;
            CUR_PARENT->die_children[CUR_PARENT->die_numchildren - 1] = current;
            CUR_PARENT->die_children[CUR_PARENT->die_numchildren] = NULL;

            current->die_parent = CUR_PARENT;
        }

        if(current->die_diename){
            write_tabs(level);
            describe_die(current);
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

    /*
    die_t *root_die = *_root_die;

    // XXX calloc so if a dwarf func fails I can still test for NULL
    if(!root_die)
        root_die = calloc(1, sizeof(die_t));
    */

    Dwarf_Die cu_rootdie;
    int ret = dwarf_siblingof_b(dwarfinfo->di_dbg, NULL, is_info,
            &cu_rootdie, &d_error);

    if(ret == DW_DLV_ERROR){
        asprintf(error, "dwarf_siblingof_b: %s",
                dwarf_errmsg_by_number(ret));
        return 1;
    }

    /*
    if((ret = copy_die_info(&root_die))){
        asprintf(error, "copy_die_info: %s",
                dwarf_errmsg_by_number(ret));
        return 1;
    }*/

    die_t *root_die = create_new_die(cu_rootdie);

    //printf("die name: '%s'\n", root_die->die_diename);
    construct_die_tree(dwarfinfo, root_die, 0);
    putchar('\n');

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
