#include <stdio.h>
#include <stdlib.h>

#include <libdwarf.h>

#include "common.h"

typedef struct {
    Dwarf_Die die_dwarfdie;

    Dwarf_Half die_tag;
    const char *die_tagname;

    Dwarf_Attribute *die_attrs;
    Dwarf_Signed die_attrcnt;

    char *die_diename;
} die_t;

static int copy_die_info(die_t **die){
    Dwarf_Error d_error;

    int ret = dwarf_diename((*die)->die_dwarfdie, &((*die)->die_diename),
            &d_error);

    if(ret)
        return ret;

    ret = dwarf_tag((*die)->die_dwarfdie, &((*die)->die_tag), &d_error);

    if(ret)
        return ret;

    ret = dwarf_get_TAG_name((*die)->die_tag, &((*die)->die_tagname));

    if(ret)
        return ret;

    ret = dwarf_attrlist((*die)->die_dwarfdie, &((*die)->die_attrs),
            &((*die)->die_attrcnt), &d_error);

    if(ret)
        return ret;

    return 0;
}

static die_t *create_new_die(Dwarf_Die based_on){
    die_t *d = calloc(1, sizeof(die_t));
    d->die_dwarfdie = based_on;

    copy_die_info(&d);

    return d;
}

static inline void write_tabs(int cnt){
    for(int i=0; i<cnt; i++)
        putchar('\t');
}

/* credit is due: simplereader.c */
static void construct_die_tree(dwarfinfo_t *dwarfinfo, Dwarf_Die die,
        int level){
    int is_info = 1;
    Dwarf_Die child_die, cur_die = die;
    Dwarf_Error d_error;

    int ret = DW_DLV_OK;

    die_t *my_die = create_new_die(cur_die);

    if(my_die->die_diename){
        write_tabs(level);
        printf("<%s>: '%s'\n", my_die->die_tagname, my_die->die_diename);
    }

    for(;;){
        ret = dwarf_child(cur_die, &child_die, NULL);

        if(ret == DW_DLV_ERROR){
            dprintf("dwarf_child level %d: %s\n", level, dwarf_errmsg_by_number(ret));
            exit(1);
        }
        else if(ret == DW_DLV_OK){
            //write_tabs(level);
            //dprintf("got a child, recursing\n");
            construct_die_tree(dwarfinfo, child_die, level+1);
            /*
            die_t *my_die = calloc(1, sizeof(die_t));
            my_die->die_dwarfdie = child_die;
            copy_die_info(&my_die);
            */

            /*die_t *my_die = create_new_die(child_die);
            if(my_die->die_diename){
                write_tabs(level);
                printf("die name: '%s'\n", my_die->die_diename);
            }*/
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
          //  write_tabs(level);
            //dprintf("done at this level, breaking\n");
            break;
        }

        if(cur_die != die){
            dwarf_dealloc(dwarfinfo->di_dbg, cur_die, DW_DLA_DIE);
        }
    
        cur_die = sibling_die;

        /*
        die_t *my_die = calloc(1, sizeof(die_t));
        my_die->die_dwarfdie = cur_die;
        copy_die_info(&my_die);
        */
        die_t *my_die = create_new_die(cur_die);
        
        if(my_die->die_diename){
            write_tabs(level);
            printf("<%s>: '%s'\n", my_die->die_tagname, my_die->die_diename);
        }
        

        
        //write_tabs(level);
        //printf("die name: '%s'\n", my_die->die_diename?my_die->die_diename:"NULL");
        
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

    die_t *root_die = *_root_die;

    // XXX calloc so if a dwarf func fails I can still test for NULL
    if(!root_die)
        root_die = calloc(1, sizeof(die_t));

    int ret = dwarf_siblingof_b(dwarfinfo->di_dbg, NULL, is_info,
            &root_die->die_dwarfdie, &d_error);

    if(ret == DW_DLV_ERROR){
        asprintf(error, "dwarf_siblingof_b: %s",
                dwarf_errmsg_by_number(ret));
        return 1;
    }

    if((ret = copy_die_info(&root_die))){
        asprintf(error, "copy_die_info: %s",
                dwarf_errmsg_by_number(ret));
        return 1;
    }

    //printf("die name: '%s'\n", root_die->die_diename);
    construct_die_tree(dwarfinfo, root_die->die_dwarfdie, 0);
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
