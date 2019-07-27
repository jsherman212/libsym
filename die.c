#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dwarf.h>
#include <libdwarf.h>

#include "common.h"
#include "compunit.h"

#define BLACK "\033[30m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define CYAN "\033[36m"
#define MAGENTA "\033[35m"
#define YELLOW "\033[33m"
#define LIGHT_GREEN "\033[92m"
#define LIGHT_MAGENTA "\033[95m"
#define LIGHT_RED "\033[91m"
#define LIGHT_YELLOW "\033[93m"
#define LIGHT_BLUE "\033[94m" // XXX purple?
#define RESET "\033[39m"

#define YELLOW_BG "\033[43m"
#define BLUE_BG "\033[44m"
#define LIGHT_YELLOW_BG "\033[103m"
#define LIGHT_RED_BG "\033[101m"
#define LIGHT_GREEN_BG "\033[102m"
#define RESET_BG "\033[49m"

typedef struct die die_t;

struct dwarf_locdesc {
    uint64_t locdesc_lopc;
    uint64_t locdesc_hipc;
    int locdesc_bounded;
    Dwarf_Small locdesc_op;
    Dwarf_Unsigned locdesc_opd1;
    Dwarf_Unsigned locdesc_opd2;
    Dwarf_Unsigned locdesc_opd3;
    Dwarf_Unsigned locdesc_offsetforbranch;

    struct dwarf_locdesc *locdesc_next;
};

struct die {
    Dwarf_Die die_dwarfdie;
    Dwarf_Unsigned die_dieoffset;

    char **die_srcfiles;
    Dwarf_Signed die_srcfilescnt;

    /* If this DIE represents a compilation unit, the following
     * two are non-NULL.
     */
    Dwarf_Line *die_srclines;
    Dwarf_Signed die_srclinescnt;

    Dwarf_Half die_tag;
    char *die_tagname;

    Dwarf_Attribute *die_attrs;
    Dwarf_Signed die_attrcnt;

    /* If this DIE represents an anonymous type. */
    int die_anon;

    /* If this DIE represents a lexical block. */
    int die_lexblock;

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

    /* top level data type DIE */
    Dwarf_Die die_datatypedie;
    Dwarf_Half die_datatypedietag;
    Dwarf_Unsigned die_databytessize;
    char *die_datatypename;

    /* If this DIE represents an inlined subroutine, the following
     * two are initialized.
     */
    int die_inlinedsub;
    Dwarf_Unsigned die_aboriginoff;

    /* Where a subroutine, lexical block, etc starts and ends */
    Dwarf_Unsigned die_low_pc;
    Dwarf_Unsigned die_high_pc;

    /* Where a member is in a structure, union, etc */
    Dwarf_Unsigned die_memb_off;

    /* If this DIE has the attribute DW_AT_location, the following
     * three are initialized.
     */
    //Dwarf_Loc_Head_c die_loclisthead;
    Dwarf_Unsigned die_loclistcnt;
    /* Will have die_loclistcnt elements */
    struct dwarf_locdesc **die_locdescs;

    /* If this DIE's tag is DW_TAG_subprogram, this will be initialized */
    struct dwarf_locdesc *die_framebaselocdesc;

    int die_dwarfdieneedsfree;
};

void die_search(die_t *, void *, int, die_t **);

static inline void write_tabs(int cnt){
    for(int i=0; i<cnt; i++)
        putchar('\t');
}

// XXX if this DIE represents a struct or union. is aggregate the right word?
static int is_aggregate_type(Dwarf_Half tag){
    return tag == DW_TAG_structure_type || tag == DW_TAG_union_type;
}

static int is_anonymous_type(die_t *die){
    return (die->die_tag == DW_TAG_structure_type ||
            die->die_tag == DW_TAG_union_type ||
            die->die_tag == DW_TAG_enumeration_type) &&
        !die->die_diename;
}

static int is_inlined_subroutine(die_t *die){
    return die->die_tag == DW_TAG_inlined_subroutine;
}

static const char *dwarf_type_tag_to_string(Dwarf_Half tag){
    switch(tag){
        case DW_TAG_const_type:
            return "const"; 
        case DW_TAG_restrict_type:
            return "restrict";
        case DW_TAG_volatile_type:
            return "volatile";
        case DW_TAG_pointer_type:
            return "*";
        default:
            return "<unknown>";
    };
}

static Dwarf_Die get_type_die(Dwarf_Debug dbg, Dwarf_Die from){
    Dwarf_Error d_error = NULL;
    Dwarf_Attribute attr = NULL;

    int ret = dwarf_attr(from, DW_AT_type, &attr, &d_error);

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return NULL;
    }

    Dwarf_Unsigned offset = 0;

    ret = dwarf_global_formref(attr, &offset, &d_error);

    dwarf_dealloc(dbg, attr, DW_DLA_ATTR);

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return NULL;
    }

    Dwarf_Die type_die = NULL;
    int is_info = 1;

    ret = dwarf_offdie_b(dbg, offset, is_info, &type_die, &d_error);

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return NULL;
    }

    return type_die;
}

static char *get_die_name_raw(Dwarf_Debug dbg, Dwarf_Die from){
    char *name = NULL;
    Dwarf_Error d_error = NULL;

    int ret = dwarf_diename(from, &name, &d_error);

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return NULL;
    }

    return name;
}

static Dwarf_Half get_die_tag_raw(Dwarf_Debug dbg, Dwarf_Die from){
    Dwarf_Half tag = 0;
    Dwarf_Error d_error = NULL;

    int ret = dwarf_tag(from, &tag, &d_error);

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return -1;
    }

    return tag;
}

static const char *get_tag_name(Dwarf_Half tag){
    const char *tag_name = NULL;
    dwarf_get_TAG_name(tag, &tag_name);

    return tag_name;
}

static Dwarf_Unsigned get_die_offset(Dwarf_Debug dbg, Dwarf_Die from){
    Dwarf_Unsigned offset = 0;
    Dwarf_Error d_error = NULL;

    int ret = dwarf_dieoffset(from, &offset, &d_error);

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return 0;
    }

    return offset;
}

static Dwarf_Die get_child_die(Dwarf_Debug dbg, Dwarf_Die parent){
    Dwarf_Die child_die = NULL;
    Dwarf_Error d_error = NULL;

    int ret = dwarf_child(parent, &child_die, &d_error);

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return NULL;
    }

    return child_die;
}

static Dwarf_Die get_sibling_die(Dwarf_Debug dbg, Dwarf_Die from){
    Dwarf_Die sibling_die = NULL;
    Dwarf_Error d_error = NULL;
    int is_info = 1;

    int ret = dwarf_siblingof_b(dbg, from, is_info, &sibling_die, &d_error);

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return NULL;
    }

    return sibling_die;
}

static int get_die_attrlist(Dwarf_Debug dbg, Dwarf_Die from,
        Dwarf_Attribute **attrlist, Dwarf_Signed *attrcnt){
    if(!attrlist || !attrcnt)
        return -1;

    Dwarf_Error d_error = NULL;

    int ret = dwarf_attrlist(from, attrlist, attrcnt, &d_error);

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return -1;
    }

    return 0;
}

static int get_die_attribute(Dwarf_Debug dbg, Dwarf_Die from,
        Dwarf_Half whichattr, Dwarf_Attribute *attr){
    if(!attr)
        return -1;

    Dwarf_Error d_error = NULL;

    int ret = dwarf_attr(from, whichattr, attr, &d_error);

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return -1;
    }

    return 0;
}

enum {
    FORMSDATA = 0,
    FORMUDATA
};

static int get_form_data_from_attr(Dwarf_Debug dbg, Dwarf_Attribute attr,
        void *data, int way){
    if(!data)
        return -1;

    Dwarf_Error d_error = NULL;
    int ret = DW_DLV_OK;

    if(way == FORMSDATA)
        ret = dwarf_formsdata(attr, ((Dwarf_Signed *)data), &d_error);
    else if(way == FORMUDATA)
        ret = dwarf_formudata(attr, ((Dwarf_Unsigned *)data), &d_error);
    else
        return -1;

    if(ret == DW_DLV_ERROR){
        dprintf("dealloc\n");
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return -1;
    }

    return 0;
}

#define NON_COMPILE_TIME_CONSTANT_SIZE ((Dwarf_Unsigned)-1)

static int lex_block_count = 0, anon_struct_count = 0, anon_union_count = 0,
           anon_enum_count = 0, IS_POINTER = 0;

static void generate_data_type_info(Dwarf_Debug dbg, void *compile_unit,
        Dwarf_Die die, char *outtype, Dwarf_Unsigned *outsize, int maxlen, int level){
    char *die_name = get_die_name_raw(dbg, die);
    Dwarf_Half die_tag = get_die_tag_raw(dbg, die);

    /* This has to be done with a global variable...
     * This variable is used to calculate data size.
     * Once we see a pointer, we cannot disregard that fact when
     * recursing/returning.
     */
    if(die_tag == DW_TAG_pointer_type){
        IS_POINTER = 1;

        /* Unlikely, but prevent setting outsize after it has been set once. */
        if(*outsize == 0 && *outsize != NON_COMPILE_TIME_CONSTANT_SIZE)
            *outsize = cu_get_address_size(compile_unit);
    }

    Dwarf_Unsigned die_offset = get_die_offset(dbg, die);

    if(die_tag == DW_TAG_formal_parameter){
        Dwarf_Die typedie = get_type_die(dbg, die);

        if(!typedie){
            write_tabs(level);
            printf("formal parameter typedie NULL? void?\n");
            return;
        }

        generate_data_type_info(dbg, compile_unit, typedie,
                outtype, outsize, maxlen, level+1);
        dwarf_dealloc(dbg, typedie, DW_DLA_DIE);

        return;
    }
    //printf("die name '%s' tag name '%s' offset %#llx\n", die_name, die_tag_name, die_offset);

    /* Function pointer */
    if(die_tag == DW_TAG_subroutine_type){
        IS_POINTER = 1;

        Dwarf_Die typedie = get_type_die(dbg, die);

        if(!typedie)
            strcat(outtype, "void");
        else{
            generate_data_type_info(dbg, compile_unit, typedie,
                    outtype, outsize, maxlen, level+1);
            dwarf_dealloc(dbg, typedie, DW_DLA_DIE);
        }

        strcat(outtype, "(");

        /* Get parameters */
        Dwarf_Die parameter_die = get_child_die(dbg, die);

        /* No parameters */
        if(!parameter_die){
            strcat(outtype, "void)");
            return;
        }

        for(;;){
            generate_data_type_info(dbg, compile_unit, parameter_die,
                    outtype, outsize, maxlen, level+1);

            Dwarf_Die sibling_die = get_sibling_die(dbg, parameter_die);

            dwarf_dealloc(dbg, parameter_die, DW_DLA_DIE);

            if(!sibling_die)
                break;

            parameter_die = sibling_die;

            strcat(outtype, ", ");
        }

        strcat(outtype, ")");

        return;
    }

    if(die_tag == DW_TAG_base_type ||
            die_tag == DW_TAG_enumeration_type ||
            die_tag == DW_TAG_structure_type ||
            die_tag == DW_TAG_union_type){
        if(die_name){
            size_t outtype_len = strlen(outtype);

            if(outtype_len + strlen(die_name) >= maxlen){
                write_tabs(level);
                printf("preventing buffer overflow, returning...\n");
                return;
            }

            strcat(outtype, die_name);
        }

        if(!IS_POINTER){
            Dwarf_Error d_error = NULL;
            int ret = dwarf_bytesize(die, outsize, &d_error);

            if(ret == DW_DLV_ERROR){
                dprintf("dealloc\n");
                dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
            }
        }

        return;
    }

    Dwarf_Attribute *attrlist = NULL;
    Dwarf_Signed attrcnt = 0;

    int attrlisterr = get_die_attrlist(dbg, die, &attrlist, &attrcnt);

    if(attrcnt == 0 && (die_tag == DW_TAG_pointer_type ||
                die_tag == DW_TAG_const_type ||
                die_tag == DW_TAG_volatile_type ||
                die_tag == DW_TAG_restrict_type)){
        /* No attributes, don't need to free */

        char tag_str[96] = {0};

        /* It seems that if a DW_TAG_*_type has no attributes,
         * it is generic, like a void pointer. However, they can have
         * qualifiers, so we have to check for that.
         */
        if(die_tag == DW_TAG_pointer_type)
            strcpy(tag_str, "void *");
        else{
            strcat(tag_str, dwarf_type_tag_to_string(die_tag));
            strcat(tag_str, " void");
        }

        if(strlen(outtype) + strlen(tag_str) >= maxlen){
            write_tabs(level);
            printf("preventing buffer overflow, returning...\n");
            return;
        }

        strcat(outtype, tag_str);
        return;
    }
    else if(attrlisterr != DW_DLV_OK){
        return;
    }

    Dwarf_Die typedie = get_type_die(dbg, die);
    //write_tabs(level);
    //printf("%#llx: typedie: %p\n", die_offset, typedie);

    // write_tabs(level);
    // printf("Level %d: got type: '"RED"%s"RESET"'\n", level, type);

    generate_data_type_info(dbg, compile_unit, typedie,
            outtype, outsize, maxlen, level+1);

    dwarf_dealloc(dbg, typedie, DW_DLA_DIE);

    if(die_tag == DW_TAG_array_type){
        /* Number of subrange DIEs denote how many dimensions */
        Dwarf_Die subrange_die = get_child_die(dbg, die);

        if(!subrange_die)
            return;

        for(;;){
            Dwarf_Attribute count_attr = NULL;

            if(get_die_attribute(dbg, subrange_die, DW_AT_count, &count_attr))
                break;

            Dwarf_Unsigned nmemb = 0;

            int ret = get_form_data_from_attr(dbg, count_attr, &nmemb, FORMUDATA);

            dwarf_dealloc(dbg, count_attr, DW_DLA_ATTR);

            if(ret){
                /* Variable length array determined at runtime */
                const char *a = "[]";
                strcat(outtype, a);

                // XXX set outsize to the array's base type
                *outsize = NON_COMPILE_TIME_CONSTANT_SIZE;
                break;
            }

            // XXX don't forget to check for overflow
            char arrdim[96] = {0};
            snprintf(arrdim, sizeof(arrdim), "[%#llx]", nmemb);
            strcat(outtype, arrdim);

            *outsize *= nmemb;

            Dwarf_Die sibling_die = get_sibling_die(dbg, subrange_die);

            dwarf_dealloc(dbg, subrange_die, DW_DLA_DIE);

            if(!sibling_die)
                break;

            subrange_die = sibling_die;
        }

        return;
    }

    const char *type_tag_string = dwarf_type_tag_to_string(die_tag);
    const size_t type_tag_len = strlen(type_tag_string);

    size_t outtype_len = strlen(outtype);

    int is_typedef = die_tag == DW_TAG_typedef;

    //write_tabs(level);
    /*
       printf("%#llx: die_tag '"GREEN"%s"RESET"' real tag '"GREEN"%s"RESET"'\n", die_offset, type_tag_string,
       get_tag_name(die_tag));
       */
    /* If our current DIE is a typedef, this function will follow
     * (and append, without this check) the typedef types in the DIE chain.
     * As we return, we'll go up the DIE chain and see the "true" type
     * last. So we replace the previous typedef with the current one.
     */
    if(is_typedef){
        //write_tabs(level);
        //printf("got a typedef '%s'\n", die_name);

        Dwarf_Die typedie = get_type_die(dbg, die);

        //write_tabs(level);
        //printf("typedie %p\n", typedie);

        char *typedeftype = get_die_name_raw(dbg, typedie);

        dwarf_dealloc(dbg, typedie, DW_DLA_DIE);

        //write_tabs(level);
        //printf("got underlying typedef type '%s'\n", typedeftype);

        size_t outtypelen = strlen(outtype);
        size_t typedeftypelen = 0;

        if(typedeftype)
            typedeftypelen = strlen(typedeftype);

        if(!typedeftype || outtypelen == 0 ||
                (outtypelen == typedeftypelen && strcmp(outtype, typedeftype) == 0)){
            strcpy(outtype, die_name);
            return;
        }

        long replaceat = outtypelen - typedeftypelen;

        if(replaceat < 0 || !typedeftype)
            replaceat = 0;

        if(replaceat > outtypelen)
            return;

        /*
           write_tabs(level);
           printf("level %d: gonna replace '"RED"%s"RESET"' with '"GREEN"%s"RESET"' at outtype[%ld], outtype: '%s'\n",
           level, typedeftype, type, replaceat, outtype);
           */


        long replacelen = outtypelen - typedeftypelen;

        if(replacelen < 0)
            replacelen = 0;

        // write_tabs(level);
        // printf("level %d, still here, gonna replace %ld bytes \n", level, replacelen);

        memset(&outtype[replaceat], 0, replacelen * sizeof(char));

        // write_tabs(level);
        // printf("outtype is now '%s', appending...\n", outtype);

        strcat(&outtype[replaceat], die_name);

        // write_tabs(level);
        // printf("appended, outtype is now '%s'\n", outtype);

        return;
    }

    int append_space = (outtype_len > 0 && outtype[outtype_len - 1] != '*');
    int overflow_amount = outtype_len + type_tag_len;

    if(append_space)
        overflow_amount++;

    if(overflow_amount >= maxlen){
        write_tabs(level);
        printf("preventing buffer overflow, returning...\n");
        return;
    }

    if(append_space)
        strcat(outtype, " ");

    strncat(outtype, type_tag_string, type_tag_len);
}

static void get_die_data_type_info(dwarfinfo_t *dwarfinfo, void *compile_unit,
        die_t **die){
    Dwarf_Debug dbg = dwarfinfo->di_dbg;
    Dwarf_Error d_error = NULL;
    Dwarf_Attribute attr = NULL;

    int ret = dwarf_attr((*die)->die_dwarfdie, DW_AT_type, &attr, &d_error);

    if(ret == DW_DLV_ERROR)
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

    if(ret != DW_DLV_OK)
        return;

    ret = dwarf_global_formref(attr, &((*die)->die_datatypedieoffset),
            &d_error);

    if(ret == DW_DLV_ERROR)
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

    dwarf_dealloc(dbg, attr, DW_DLA_ATTR);

    ret = dwarf_offdie(dwarfinfo->di_dbg, (*die)->die_datatypedieoffset,
            &((*die)->die_datatypedie), &d_error);

    if(ret == DW_DLV_ERROR)
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

    if(ret != DW_DLV_OK)
        return;

    dwarf_tag((*die)->die_datatypedie,
            &((*die)->die_datatypedietag), &d_error);

    Dwarf_Half tag = (*die)->die_datatypedietag;

    /* If this DIE already represents a base type (int, double, etc)
     * or an enum, we're done.
     */
    if(tag == DW_TAG_base_type || tag == DW_TAG_enumeration_type){
        ret = dwarf_diename((*die)->die_datatypedie, &((*die)->die_datatypename),
                &d_error);

        if(ret == DW_DLV_ERROR)
            dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

        ret = dwarf_bytesize((*die)->die_datatypedie, &((*die)->die_databytessize),
                &d_error);

        if(ret == DW_DLV_ERROR)
            dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
    }
    else{
        /* Otherwise, we have to follow the chain of DIEs that make up this
         * data type. For example:
         *      char **argv;
         *
         *      DW_TAG_pointer_type ->
         *          DW_TAG_pointer_type ->
         *              DW_TAG_base_type (char)
         */
        enum { maxlen = 256 };

        char name[maxlen] = {0};
        Dwarf_Unsigned size = 0;

        generate_data_type_info(dwarfinfo->di_dbg, compile_unit,
                (*die)->die_datatypedie, name, &size, maxlen, 0);

        IS_POINTER = 0;

        (*die)->die_databytessize = size;
        (*die)->die_datatypename = strdup(name);
    }
}

static die_t *CUR_PARENTS[1000] = {0};

static void describe_die_internal(die_t *, int);
static void copy_location_lists(Dwarf_Debug dbg, die_t **die,
        Dwarf_Half whichattr, int level){
    Dwarf_Attribute attr = NULL;
    get_die_attribute(dbg, (*die)->die_dwarfdie, whichattr, &attr);

    if(!attr){
        //dprintf("no DW_AT_location on this DIE\n");
        return;
    }

    Dwarf_Error d_error = NULL;

    /*
    Dwarf_Half form = 0;
    dwarf_whatform(locexpr_attr, &form, &d_error);
    const char *fname = NULL;
    dwarf_get_FORM_name(form, &fname);

    dprintf("Form '%s'\n", fname);
    */

    Dwarf_Loc_Head_c loclisthead = NULL;
    int lret = dwarf_get_loclist_c(attr, &loclisthead,//&((*die)->die_loclisthead),
            &((*die)->die_loclistcnt), &d_error);

    dwarf_dealloc(dbg, attr, DW_DLA_ATTR);

    if(lret == DW_DLV_OK){
        Dwarf_Unsigned lcount = (*die)->die_loclistcnt;

        (*die)->die_locdescs = calloc(lcount, sizeof(struct dwarf_locdesc));

        for(Dwarf_Unsigned i=0; i<lcount; i++){
            Dwarf_Small loclist_source = 0, lle_value = 0;
            Dwarf_Addr lopc = 0, hipc = 0;
            Dwarf_Unsigned ulocentry_count = 0, section_offset = 0,
                           locdesc_offset = 0;
            Dwarf_Locdesc_c locentry = NULL;

            /* d_error is still NULL */

            lret = dwarf_get_locdesc_entry_c(loclisthead,//(*die)->die_loclisthead,
                    i, &lle_value, &lopc, &hipc, &ulocentry_count,
                    &locentry, &loclist_source, &section_offset,
                    &locdesc_offset, &d_error);
            if(lret == DW_DLV_OK){
                for(Dwarf_Unsigned j=0; j<ulocentry_count; j++){
                    Dwarf_Small op = 0;
                    Dwarf_Unsigned opd1 = 0, opd2 = 0, opd3 = 0,
                                   offsetforbranch = 0;
                    
                    /* d_error is still NULL */

                    int opret = dwarf_get_location_op_value_c(locentry,
                            j, &op, &opd1, &opd2, &opd3, &offsetforbranch,
                            &d_error);

                    if(opret == DW_DLV_OK){
                        //write_tabs(level);
        //                dprintf("[i: %lld j: %lld]: expr offset: %#llx loclist_source: %d lle_value: %d op: 0x%04x opd1: %lld opd2: %lld opd3: %lld offsetForBranch: %lld lopc: %#llx hipc: %#llx\n",
          //                      i, j, section_offset, loclist_source, lle_value, op, opd1, opd2, opd3,
            //                    offsetforbranch, lopc, hipc);

                        struct dwarf_locdesc *locdesc =
                            calloc(1, sizeof(struct dwarf_locdesc));

                        enum {
                            LOCATION_EXPRESSION = 0,
                            LOCATION_LIST_ENTRY,
                            LOCATION_LIST_ENTRY_SPLIT
                        };

                        /* Low and high PC values here are based off the
                         * compilation unit's (or root DIE) low PC value when
                         * loclist_source == LOCATION_LIST_ENTRY. Otherwise,
                         * lle_value, lopc, and hipc aren't of any use to us.
                         */
                        if(loclist_source == LOCATION_LIST_ENTRY){
                            die_t *cudie = CUR_PARENTS[0];

                            if(!cudie){
                                dprintf("CU DIE NULL? how???\n");
                                abort();
                            }

                            //write_tabs(level);
                            //printf("Found CU DIE: '%s'\n", cudie->die_diename);

                            //lopc += cudie->die_low_pc;
                            //hipc += cudie->die_low_pc;

                            locdesc->locdesc_lopc = lopc + cudie->die_low_pc;
                            locdesc->locdesc_hipc = hipc + cudie->die_low_pc;

                            locdesc->locdesc_bounded = 1;
                        }

                        locdesc->locdesc_op = op;
                        locdesc->locdesc_opd1 = opd1;
                        locdesc->locdesc_opd2 = opd2;
                        locdesc->locdesc_opd3 = opd3;
                        locdesc->locdesc_offsetforbranch = offsetforbranch;

                        if(j > 0){
                            struct dwarf_locdesc *current = NULL;

                            if(whichattr == DW_AT_location)
                                current = (*die)->die_locdescs[i];
                            else{
                                dprintf("j>0: called with whichattr %#x\n", whichattr);
                                // XXX
                                abort();
                            }

                            while(current->locdesc_next)
                                current = current->locdesc_next;

                            current->locdesc_next = locdesc;

                            /*
                            struct dwarf_locdesc *ld = (*die)->die_locdescs[i];

                            while(ld){
                                int bkpthere = 0;

                                ld = ld->locdesc_next;
                            }

                            exit(0);
                            */
                        }
                        else{
                            if(whichattr == DW_AT_location)
                                (*die)->die_locdescs[i] = locdesc;
                            else if(whichattr == DW_AT_frame_base)
                                (*die)->die_framebaselocdesc = locdesc;
                            else{
                                dprintf("Called with whichattr %#x\n", whichattr);
                                // XXX
                                abort();
                            }
                            //dprintf("(*die)->die_locdescs[i] %p\n", (*die)->die_locdescs[i]);
                        }
                    }
                    else{
                        dprintf("dwarf_get_location_op_value_c: %s\n", dwarf_errmsg_by_number(dwarf_errno(d_error)));
                        // XXX
                        exit(0);
                    }
                }
            }
            else{
                dprintf("dwarf_get_locdesc_entry_c: %s\n", dwarf_errmsg_by_number(dwarf_errno(d_error)));
                // XXX
                exit(0);
            }
        }
    }
    else if(lret == DW_DLV_ERROR){
        dprintf("dwarf_get_loclist_c: %s\n", dwarf_errmsg_by_number(dwarf_errno(d_error)));
        // XXX
        exit(0);
    }

    /* If this DIE is the child of a subroutine DIE, initialize its
     * frame base location description.
     */
    if((*die)->die_tag != DW_TAG_subprogram && level > 0){
   //     dprintf("level %d\n", level);

        int pos = level;
        die_t *curparent = CUR_PARENTS[pos];

        while(pos >= 0 &&
                (!curparent || curparent->die_tag != DW_TAG_subprogram)){
//            describe_die_internal(curparent, level);
            curparent = CUR_PARENTS[pos--];
        }

  //      describe_die_internal(curparent, level);
        if(curparent->die_tag == DW_TAG_subprogram)
            (*die)->die_framebaselocdesc = curparent->die_framebaselocdesc;

        int bkpthere = 0;
    }
}

static int copy_die_info(dwarfinfo_t *dwarfinfo, void *compile_unit,
        die_t **die, int level){
    Dwarf_Debug dbg = dwarfinfo->di_dbg;
    Dwarf_Error d_error = NULL;

    int ret = dwarf_diename((*die)->die_dwarfdie, &((*die)->die_diename),
            &d_error);

    if(ret == DW_DLV_ERROR)
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

    ret = dwarf_dieoffset((*die)->die_dwarfdie, &((*die)->die_dieoffset),
            &d_error);

    if(ret == DW_DLV_ERROR)
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

    ret = dwarf_srcfiles((*die)->die_dwarfdie, &((*die)->die_srcfiles),
            &((*die)->die_srcfilescnt), &d_error);

    if(ret == DW_DLV_ERROR)
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

    dwarf_tag((*die)->die_dwarfdie, &((*die)->die_tag), &d_error);

    if(ret == DW_DLV_ERROR)
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

    // XXX: inside iosdbg get rid of this auto naming so I can test
    // for NULL names for anonymous types
    if(is_anonymous_type(*die)){
        (*die)->die_anon = 1;

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

        Dwarf_Attribute typeattr = NULL;
        int ret = dwarf_attr((*die)->die_dwarfdie, DW_AT_abstract_origin,
                &typeattr, &d_error);

        if(ret == DW_DLV_OK){
            ret = dwarf_global_formref(typeattr, &((*die)->die_aboriginoff),
                    &d_error);

            if(ret == DW_DLV_ERROR)
                dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

            dwarf_dealloc(dbg, typeattr, DW_DLA_ATTR);
        }
    }

    dwarf_get_TAG_name((*die)->die_tag, (const char **)&((*die)->die_tagname));

    /* Label these ourselves */
    if(!(*die)->die_diename){
        if((*die)->die_tag == DW_TAG_lexical_block){
            asprintf(&((*die)->die_diename),
                    "LEXICAL_BLOCK_%d",// (auto-named by libsym)",
                    lex_block_count++);
            (*die)->die_lexblock = 1;
        }
    }

    ret = dwarf_attrlist((*die)->die_dwarfdie, &((*die)->die_attrs),
            &((*die)->die_attrcnt), &d_error);

    if(ret == DW_DLV_ERROR)
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

    dwarf_die_abbrev_children_flag((*die)->die_dwarfdie,
            &((*die)->die_haschildren));

    get_die_data_type_info(dwarfinfo, compile_unit, die);

    ret = dwarf_lowpc((*die)->die_dwarfdie, &((*die)->die_low_pc), &d_error);

    if(ret == DW_DLV_ERROR)
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

    Dwarf_Half retform = 0;
    enum Dwarf_Form_Class retformclass = 0;
    ret = dwarf_highpc_b((*die)->die_dwarfdie, &((*die)->die_high_pc), &retform,
            &retformclass, &d_error);

    if(ret == DW_DLV_ERROR)
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);

    (*die)->die_high_pc += (*die)->die_low_pc;

    Dwarf_Attribute memb_attr = NULL;
    get_die_attribute(dbg, (*die)->die_dwarfdie, DW_AT_data_member_location,
            &memb_attr);

    // XXX check for location list once expression evaluator is done
    if(memb_attr)
        get_form_data_from_attr(dbg, memb_attr, &((*die)->die_memb_off), FORMUDATA);

    dwarf_dealloc(dbg, memb_attr, DW_DLA_ATTR);

    copy_location_lists(dbg, die, DW_AT_location, level);
    copy_location_lists(dbg, die, DW_AT_frame_base, level);
    /*
    Dwarf_Attribute locexpr_attr = NULL;
    get_die_attribute(dbg, (*die)->die_dwarfdie, DW_AT_location,
            &locexpr_attr);

    if(locexpr_attr){
        ret = dwarf_formexprloc(locexpr_attr, &((*die)->die_locexprlen),
                &((*die)->die_locexpr), &d_error);

        if(ret == DW_DLV_ERROR){
            printf("%s\n", dwarf_errmsg_by_number(dwarf_errno(d_error)));
            exit(0);
            dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        }
    }

    dwarf_dealloc(dbg, locexpr_attr, DW_DLA_ATTR);
    */

    return 0;
}

static Dwarf_Half die_has_children(Dwarf_Die die){
    Dwarf_Half result = 0;
    dwarf_die_abbrev_children_flag(die, &result);

    return result;
}

static inline void write_spaces(int count){
    for(int i=0; i<count; i++)
        putchar(' ');
}

static void describe_die_locdesc_internal(struct dwarf_locdesc *locdesc,
        int is_fb, int idx, int idx2, int level, int *byteswritten){
    write_tabs(level);
    write_spaces(level+4);

    int add = 0;

    if(is_fb){
        printf(RED"|"RESET" DW_AT_frame_base:%n", &add);
        *byteswritten += (add - strlen(RED) - strlen(RESET));
    }
    else{
        printf(RED"|"RESET" locdesc ["GREEN"%d"RESET"]["RED"%d"RESET"]:%n", idx, idx2, &add);
        *byteswritten += (add - strlen(RED) - strlen(RESET) - strlen(GREEN) - strlen(RESET) -
                    strlen(RED) - strlen(RESET));
    }

    printf(" bounded: "YELLOW"%d"RESET"%n", locdesc->locdesc_bounded, &add);
    *byteswritten += (add - strlen(YELLOW) - strlen(RESET));

    if(locdesc->locdesc_bounded){
        printf(" lopc = "LIGHT_BLUE"%#llx"RESET", hipc = "LIGHT_YELLOW"%#llx"RESET"%n",
                locdesc->locdesc_lopc, locdesc->locdesc_hipc, &add);
        *byteswritten += (add - strlen(LIGHT_BLUE) - strlen(RESET) - strlen(LIGHT_YELLOW) - strlen(RESET));
    }

    printf(", op = "CYAN"0x%04x"RESET"%n", locdesc->locdesc_op, &add);
    *byteswritten += (add - strlen(CYAN) - strlen(RESET));

    printf(", opd1 = "LIGHT_YELLOW_BG""BLACK"%s0x%llx"RESET""RESET_BG"%n",
            (long)locdesc->locdesc_opd1<0?"-":"",
            (long)locdesc->locdesc_opd1<0?(long)-locdesc->locdesc_opd1:locdesc->locdesc_opd1, &add);
    *byteswritten += (add - strlen(LIGHT_YELLOW_BG) - strlen(BLACK) - strlen(RESET) - strlen(RESET_BG));

    printf(", opd2 = "LIGHT_YELLOW_BG""BLACK"%s0x%llx"RESET""RESET_BG"%n",
            (long)locdesc->locdesc_opd2<0?"-":"",
            (long)locdesc->locdesc_opd2<0?(long)-locdesc->locdesc_opd2:locdesc->locdesc_opd2, &add);
    *byteswritten += (add - strlen(LIGHT_YELLOW_BG) - strlen(BLACK) - strlen(RESET) - strlen(RESET_BG));

    printf(", opd3 = "LIGHT_YELLOW_BG""BLACK"%s0x%llx"RESET""RESET_BG"%n",
            (long)locdesc->locdesc_opd3<0?"-":"",
            (long)locdesc->locdesc_opd3<0?(long)-locdesc->locdesc_opd3:locdesc->locdesc_opd3, &add);
    *byteswritten += (add - strlen(LIGHT_YELLOW_BG) - strlen(BLACK) - strlen(RESET) - strlen(RESET_BG));

    printf(", offsetforbranch = "LIGHT_GREEN_BG""BLACK"%s0x%llx"RESET""RESET_BG"%n",
            (long)locdesc->locdesc_offsetforbranch<0?"-":"",
            (long)locdesc->locdesc_offsetforbranch<0?(long)-locdesc->locdesc_offsetforbranch:locdesc->locdesc_offsetforbranch, &add);
    *byteswritten += (add - strlen(LIGHT_YELLOW_BG) - strlen(BLACK) - strlen(RESET) - strlen(RESET_BG));
}

static void describe_die_internal(die_t *die, int level){
    if(!die)
        return;

    const char *varnamecolorstr = LIGHT_MAGENTA;

    if(die->die_haschildren)
        varnamecolorstr = GREEN;

    printf("%#llx: <%d> <%s>: '%s%s%s', is parent: %d, type DIE at %s%#llx%s",
            die->die_dieoffset, level, die->die_tagname,
            varnamecolorstr, die->die_diename, RESET,
            die->die_haschildren, die->die_datatypedieoffset!=0?CYAN:"",
            die->die_datatypedieoffset, die->die_datatypedieoffset!=0?RESET:"");

    if(die->die_datatypedieoffset!=0){
        printf(", type = '"LIGHT_BLUE"%s"RESET"'", die->die_datatypename);

        if(die->die_tag != DW_TAG_subprogram){
            printf(", sizeof(%s%s%s) = "LIGHT_YELLOW"%#llx"RESET"",
                    varnamecolorstr, die->die_diename, RESET, die->die_databytessize);
        }
    }

    if(die->die_tag == DW_TAG_compile_unit ||
            die->die_tag == DW_TAG_subprogram ||
            die->die_tag == DW_TAG_lexical_block){
        printf(", low PC = "YELLOW"%#llx"RESET", high PC = "YELLOW"%#llx"RESET"",
                die->die_low_pc, die->die_high_pc);
    }

    if(die->die_tag == DW_TAG_member){
        char *parentname = die->die_parent->die_diename;
        printf(", offset = "GREEN"%s"RESET"+"LIGHT_GREEN"%#llx"RESET"",
                parentname, die->die_memb_off);
    }

    if(level == 0){
        printf(", srclinescnt = "MAGENTA"%lld"RESET"", die->die_srclinescnt);
    }

   if(die->die_loclistcnt > 0){ 
        printf(", loclistcnt = %s%#llx%s",
                BLUE_BG, die->die_loclistcnt, RESET_BG);
   }

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

    int putseparator = 0;
    int maxbyteswritten = 0;

    if(die->die_loclistcnt > 0){
        putseparator = 1;

        for(Dwarf_Unsigned i=0; i<die->die_loclistcnt; i++){
            struct dwarf_locdesc *current = die->die_locdescs[i];

            int idx2 = 0;
            while(current){
                int byteswritten = 0;
                describe_die_locdesc_internal(current, 0, i, idx2, level, &byteswritten);

                if(byteswritten > maxbyteswritten)
                    maxbyteswritten = byteswritten;

                putchar('\n');
                idx2++;
                current = current->locdesc_next;
            }
        }
    }

    if(die->die_framebaselocdesc){
        int byteswritten = 0;
        describe_die_locdesc_internal(die->die_framebaselocdesc, 1, 0, 0, level, &byteswritten);
        if(byteswritten > maxbyteswritten)
            maxbyteswritten = byteswritten;

        putseparator = 1;
        putchar('\n');
    }

    if(putseparator){
        write_tabs(level);
        write_spaces(level+4);

        printf(LIGHT_YELLOW"* "LIGHT_RED);

        /* Account for the printf call above */
        maxbyteswritten -= 2;
        for(int i=0; i<maxbyteswritten; i++)
            putchar('-');

        printf(RESET"\n");
    }
}

static die_t *create_new_die(dwarfinfo_t *dwarfinfo, void *compile_unit,
        Dwarf_Die based_on, int level){
    die_t *d = calloc(1, sizeof(die_t));
    d->die_dwarfdie = based_on;

    copy_die_info(dwarfinfo, compile_unit, &d, level);

    if(d->die_haschildren){
        d->die_children = malloc(sizeof(die_t));
        d->die_children[0] = NULL;

        d->die_numchildren = 0;
    }

    return d;
}

static int should_add_die_to_tree(die_t *die){
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

static void add_die_to_tree(die_t *current, int level){
    if(level == 0){
        CUR_PARENTS[level] = current;
        return;
    }

    if(current->die_haschildren){
        CUR_PARENTS[level] = current;

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
    else{
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

   // write_tabs(level);
   // describe_die_internal(current, level);
}

static void die_free(Dwarf_Debug dbg, die_t *die, int ending){
    if(!die)
        return;

    if(ending)
        dwarf_dealloc(dbg, die->die_dwarfdie, DW_DLA_DIE);
    else{
        free(die->die_children);
        die->die_children = NULL;
    }

    for(Dwarf_Signed i=0; i<die->die_srcfilescnt; i++)
        dwarf_dealloc(dbg, die->die_srcfiles[i], DW_DLA_STRING);

    dwarf_dealloc(dbg, die->die_srcfiles, DW_DLA_LIST);
    dwarf_srclines_dealloc(dbg, die->die_srclines, die->die_srclinescnt);

    for(Dwarf_Signed i=0; i<die->die_attrcnt; i++)
        dwarf_dealloc(dbg, die->die_attrs[i], DW_DLA_ATTR);

    dwarf_dealloc(dbg, die->die_attrs, DW_DLA_LIST);

    if(!die->die_anon && !die->die_lexblock)
        dwarf_dealloc(dbg, die->die_diename, DW_DLA_STRING);
    else{
        free(die->die_diename);
        die->die_diename = NULL;
    }

    dwarf_dealloc(dbg, die->die_datatypedie, DW_DLA_DIE);

    /* see get_die_data_type_info */
    if(die->die_datatypedietag == DW_TAG_base_type ||
            die->die_datatypedietag == DW_TAG_enumeration_type){
        dwarf_dealloc(dbg, die->die_datatypename, DW_DLA_STRING);
    }
    else{
        free(die->die_datatypename);
    }
}

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
static void construct_die_tree(dwarfinfo_t *dwarfinfo, void *compile_unit,
        die_t *root, die_t *parent, die_t *current, int level){
    int is_info = 1;
    Dwarf_Die child_die = NULL, cur_die = current->die_dwarfdie;
    Dwarf_Die cur_die_backup = NULL;
    Dwarf_Error d_error = NULL;

    int ret = DW_DLV_OK;

    /* We don't want to deallocate the DWARF DIE associated with the
     * parameter because it is used in dwarf_siblingof_b.
     */
    int ending = 0;

    if(should_add_die_to_tree(current)){
        add_die_to_tree(current, level);
        current->die_dwarfdieneedsfree = 0;
    }
    else{
        current->die_dwarfdieneedsfree = 1;
        die_free(dwarfinfo->di_dbg, current, ending);
        free(current);
        current = NULL;
    }

    for(;;){
        ret = dwarf_child(cur_die, &child_die, NULL);

        if(ret == DW_DLV_ERROR)
            dprintf("dwarf_child level %d: %s\n", level, dwarf_errmsg_by_number(ret));
        else if(ret == DW_DLV_OK){
            die_t *cd = create_new_die(dwarfinfo, compile_unit, child_die, level);
            construct_die_tree(dwarfinfo, compile_unit, root, parent, cd, level+1);
        }

        Dwarf_Die sibling_die = NULL;
        ret = dwarf_siblingof_b(dwarfinfo->di_dbg, cur_die, is_info,
                &sibling_die, &d_error);

        if(ret == DW_DLV_ERROR)
            dprintf("dwarf_siblingof_b level %d: %s\n", level, dwarf_errmsg_by_number(ret));
        else if(ret == DW_DLV_NO_ENTRY){
            /* Discard the parent we were on */
            CUR_PARENTS[level] = NULL;
            return;
        }

        cur_die = sibling_die;

        die_t *newdie = create_new_die(dwarfinfo, compile_unit, cur_die, level);

        if(should_add_die_to_tree(newdie)){
            add_die_to_tree(newdie, level);
            newdie->die_dwarfdieneedsfree = 0;
        }
        else{
            newdie->die_dwarfdieneedsfree = 1;
            die_free(dwarfinfo->di_dbg, newdie, ending);
            free(newdie);
            newdie = NULL;
        }
    }
}

static void display_die_tree_internal(die_t *die, int level){
    if(!die)
        return;

    write_tabs(level);
    describe_die_internal(die, level);

    if(!die->die_haschildren)
        return;
    else{
        int idx = 0;
        die_t *child = die->die_children[idx];

        while(child){
            display_die_tree_internal(child, level+1);
            child = die->die_children[++idx];
        }
    }
}

void die_display(die_t *die){
    describe_die_internal(die, 0);
}

void die_display_die_tree_starting_from(die_t *die){
    display_die_tree_internal(die, 0);
}

void die_tree_free(Dwarf_Debug dbg, die_t *die, int level){
    int ending = 1;
    die_free(dbg, die, ending);

    if(!die->die_haschildren)
        return;
    else{
        int idx = 0;
        die_t *child = die->die_children[idx];

        while(child){
            die_tree_free(dbg, child, level+1);
            free(die->die_children[idx]);
            die->die_children[idx] = NULL;
            child = die->die_children[++idx];
        }

        free(die->die_children);
        die->die_children = NULL;
    }
}

char *die_get_name(die_t *die){
    if(!die)
        return NULL;

    return die->die_diename;
}

uint64_t die_get_high_pc(die_t *die){
    if(!die)
        return 0;

    return die->die_high_pc;
}

static char *get_dwarf_line_filename(Dwarf_Debug dbg, Dwarf_Line line){
    if(!line)
        return NULL;

    Dwarf_Error d_error = NULL;
    char *filename = NULL;

    int ret = dwarf_linesrc(line, &filename, &d_error);

    if(ret == DW_DLV_ERROR){
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return NULL;
    }

    return filename;
}

static Dwarf_Unsigned get_dwarf_line_lineno(Dwarf_Debug dbg, Dwarf_Line line){
    if(!line)
        return 0;

    Dwarf_Error d_error = NULL;
    Dwarf_Unsigned lineno = 0;

    int ret = dwarf_lineno(line, &lineno, &d_error);

    if(ret == DW_DLV_ERROR){
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return 0;
    }

    return lineno;
}

static Dwarf_Addr get_dwarf_line_virtual_addr(Dwarf_Debug dbg, Dwarf_Line line){
    if(!line)
        return 0;

    Dwarf_Error d_error = NULL;
    Dwarf_Addr lineaddr = 0;

    int ret = dwarf_lineaddr(line, &lineaddr, &d_error);

    if(ret == DW_DLV_ERROR){
        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
        return 0;
    }

    return lineaddr;
}

void die_get_line_info_from_pc(Dwarf_Debug dbg, die_t *die, uint64_t pc,
        char **srcfilename, char **srcfunction, uint64_t *srclineno){
    /* Not a compilation unit DIE */
    if(!die || !die->die_srclines)
        return;

    for(Dwarf_Signed i=0; i<die->die_srclinescnt; i++){
        Dwarf_Line line = die->die_srclines[i];

        if(pc == get_dwarf_line_virtual_addr(dbg, line)){
            *srcfilename = get_dwarf_line_filename(dbg, line);
            *srclineno = get_dwarf_line_lineno(dbg, line);

            die_t *fxndie = NULL;
            die_search(die, (void *)pc, DIE_SEARCH_FUNCTION_BY_PC, &fxndie);

            // XXX concat(outbuffer...
            if(!fxndie){
                dprintf("Couldn't find function DIE with pc %#llx????\n", pc);
                return;
            }

            *srcfunction = fxndie->die_diename;

            return;
        }
    }
}

uint64_t die_get_low_pc(die_t *die){
    if(!die)
        return 0;

    return die->die_low_pc;
}

die_t **die_get_parameters(die_t *die, int *len){
    /* not a function DIE */
    if(!die || die->die_tag != DW_TAG_subprogram)
        return NULL;

    /* We don't have to recurse farther down the DIE chain,
     * parameters will be direct descendants.
     */
    die_t **params = malloc(sizeof(die_t));

    int idx = 0;
    die_t *child = die->die_children[idx];

    while(child){
        if(child->die_tag == DW_TAG_formal_parameter){
            die_t **params_rea = realloc(params, sizeof(die_t) * ++(*len));
            params = params_rea;
            params[(*len) - 1] = child;
            params[*len] = NULL;
        }

        child = die->die_children[++idx];
    }

    return params;
}

uint64_t die_lineno_to_pc(Dwarf_Debug dbg, die_t *die, uint64_t *lineno){
    if(!die || !die->die_srclines)
        return 0;

    if(!lineno)
        return 0;

    Dwarf_Error d_error = NULL;

    /* Find the closest line to lineno. Sometimes the source file does
     * not accurately reflect the compiled program.
     */
    uint64_t closestlineno = 0;
    Dwarf_Line closestline = NULL;

    uint64_t linepassedin = *lineno;

    for(Dwarf_Signed i=0; i<die->die_srclinescnt; i++){
        Dwarf_Line line = die->die_srclines[i];
        Dwarf_Unsigned curlineno = get_dwarf_line_lineno(dbg, line);

        uint64_t current = llabs((int64_t)(closestlineno - linepassedin));
        uint64_t diff = llabs((int64_t)(curlineno - linepassedin));

        /* exact match */
        if(diff == 0)
            return get_dwarf_line_virtual_addr(dbg, line);
        else if(diff < current){
            closestlineno = curlineno;
            closestline = line;
        }
    }

    // XXX concat(outbuffer...
    printf("Line %lld doesn't exist, auto-adjusted to line %lld\n",
            linepassedin, closestlineno);
    *lineno = closestlineno;

    return get_dwarf_line_virtual_addr(dbg, closestline);
}

static int die_is_func_in_range(die_t *die, void *pc){
    return die->die_tag == DW_TAG_subprogram &&
        (uint64_t)pc >= die->die_low_pc && (uint64_t)pc <= die->die_high_pc;
}

static int die_name_matches(die_t *die, void *name){
    return die->die_diename && strcmp(die->die_diename, (const char *)name) == 0;
}

static void die_search_internal(die_t *die, void *data,
        int (*comparefxn)(die_t *, void *), die_t **out){
    if(*out || !die)
        return;

    if(comparefxn(die, data)){
        *out = die;
        return;
    }

    if(!die->die_haschildren)
        return;
    else{
        int idx = 0;
        die_t *child = die->die_children[idx];

        while(child){
            die_search_internal(child, data, comparefxn, out);
            child = die->die_children[++idx];
        }
    }
}

void die_search(die_t *start, void *data, int way, die_t **out){
    int (*comparefxn)(die_t *, void *) = NULL;

    if(way == DIE_SEARCH_IF_NAME_MATCHES)
        comparefxn = die_name_matches;
    else if(way == DIE_SEARCH_FUNCTION_BY_PC)
        comparefxn = die_is_func_in_range;

    die_search_internal(start, data, comparefxn, out);
}

int initialize_and_build_die_tree_from_root_die(dwarfinfo_t *dwarfinfo,
        void *compile_unit, die_t **_root_die, char **error){
    int is_info = 1;
    Dwarf_Error d_error = NULL;

    Dwarf_Die cu_rootdie = NULL;
    int ret = dwarf_siblingof_b(dwarfinfo->di_dbg, NULL, is_info,
            &cu_rootdie, &d_error);

    if(ret == DW_DLV_ERROR){
        asprintf(error, "dwarf_siblingof_b: %s",
                dwarf_errmsg_by_number(ret));
        return 1;
    }

    die_t *root_die = create_new_die(dwarfinfo, compile_unit, cu_rootdie, 0);
    memset(CUR_PARENTS, 0, sizeof(CUR_PARENTS));
    CUR_PARENTS[0] = root_die;

    // if(strcmp(root_die->die_diename, "source/thread.c") != 0)
    //   return 0;

    construct_die_tree(dwarfinfo, compile_unit, root_die, NULL, root_die, 0);

    if(dwarf_srclines(root_die->die_dwarfdie, &root_die->die_srclines,
                &root_die->die_srclinescnt, &d_error)){
        printf("dwarf_srclines: %s\n", dwarf_errmsg_by_number(dwarf_errno(d_error)));
    }

    printf("output of display_die_tree:\n\n");

    display_die_tree_internal(root_die, 0);

    printf("end display_die_tree output\n\n");

    *_root_die = root_die;

    return 0;
}
