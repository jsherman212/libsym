#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dwarf.h>
#include <libdwarf.h>

#include "common.h"
#include "compunit.h"
#include "dexpr.h"
#include "symerr.h"

typedef struct die die_t;

enum {
    DTC_POINTER =           (1 << 0),
    DTC_STRUCT_OR_UNION =   (1 << 1),
    DTC_ARRAY =             (1 << 2)
};

struct die {
    Dwarf_Die die_dwarfdie;
    Dwarf_Unsigned die_dieoffset;

    /* If this DIE represents a compilation unit, the following
     * two are non-NULL.
     */
    Dwarf_Line *die_srclines;
    Dwarf_Signed die_srclinescnt;

    Dwarf_Half die_tag;
    char *die_tagname;

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
     * debugged program, the following ten are initialized.
     */
    Dwarf_Unsigned die_datatypedieoffset;
    Dwarf_Unsigned die_basedatatypedieoffset;
    Dwarf_Die die_datatypedie;
    Dwarf_Half die_datatypedietag;
    /* For example, we may have a struct hidden behind a typedef */
    Dwarf_Half die_basedatatypedietag;
    /* DW_ATE_* */
    Dwarf_Half die_datatypeencoding;
    Dwarf_Unsigned die_databytessize;
    char *die_datatypename;
    /* If we have an array, we need to know the size of each element,
     * not just the overall size of the array.
     */
    Dwarf_Unsigned die_arrmembsz;
    /* High level data type classification. Really, we are only interested
     * in if this data type DIE represents a pointer, struct, union,
     * array, or base type.
     */
    unsigned int die_datatypeclass : 3;

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
     * two are initialized.
     */
    Dwarf_Unsigned die_loclistcnt;

    /* Will have die_loclistcnt elements */
    void **die_loclists;

    /* If this DIE's tag is DW_TAG_subprogram, this will be initialized */
    void *die_framebaselocdesc;

    int die_dwarfdieneedsfree;
};

int die_search(die_t *, void *, int, die_t **, sym_error_t *);
int die_pc_to_lineno(Dwarf_Debug, die_t *, uint64_t, uint64_t *, sym_error_t *);

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
        Dwarf_Die die, char *outtype, Dwarf_Unsigned *outsize,
        Dwarf_Half *base_tag, Dwarf_Die *base_die,
        Dwarf_Half *base_die_encoding, Dwarf_Unsigned *base_data_type_offset,
        Dwarf_Unsigned *arrmembsz, Dwarf_Half *arrmembencoding,
        unsigned int *classification, int maxlen, int level){
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
            cu_get_address_size(compile_unit, (Dwarf_Half *)outsize, NULL);
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
                outtype, outsize, base_tag, base_die, base_die_encoding,
                base_data_type_offset, arrmembsz, arrmembencoding,
                classification, maxlen, level+1);

        if(typedie != *base_die)
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
                    outtype, outsize, base_tag, base_die, base_die_encoding,
                    base_data_type_offset, arrmembsz, arrmembencoding,
                    classification, maxlen, level+1);

            if(typedie != *base_die)
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
                    outtype, outsize, base_tag, base_die, base_die_encoding,
                    base_data_type_offset, arrmembsz, arrmembencoding,
                    classification, maxlen, level+1);

            Dwarf_Die sibling_die = get_sibling_die(dbg, parameter_die);

            if(parameter_die != *base_die)
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
        *base_tag = die_tag;
        *base_die = die;
        *base_data_type_offset = get_die_offset(dbg, die);

        if(die_tag == DW_TAG_base_type && !IS_POINTER){
            Dwarf_Unsigned off = get_die_offset(dbg, die);
            //dprintf("base type for DIE %#llx\n", off);
            Dwarf_Attribute dw_at_encoding_attr = NULL;
            get_die_attribute(dbg, die, DW_AT_encoding, &dw_at_encoding_attr);

            if(dw_at_encoding_attr){
                get_form_data_from_attr(dbg, dw_at_encoding_attr,
                        base_die_encoding, FORMSDATA);
                dwarf_dealloc(dbg, dw_at_encoding_attr, DW_DLA_ATTR);
            }
        }

        if(die_tag == DW_TAG_structure_type || die_tag == DW_TAG_union_type)
            *classification |= DTC_STRUCT_OR_UNION;

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

            if(ret == DW_DLV_ERROR)
                dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
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
            outtype, outsize, base_tag, base_die, base_die_encoding,
            base_data_type_offset, arrmembsz, arrmembencoding,
            classification, maxlen, level+1);

    if(typedie != *base_die)
        dwarf_dealloc(dbg, typedie, DW_DLA_DIE);

    if(die_tag == DW_TAG_array_type){
        *classification |= DTC_ARRAY;

        /* Number of subrange DIEs denote how many dimensions */
        Dwarf_Die subrange_die = get_child_die(dbg, die);

        if(!subrange_die)
            return;

        for(;;){
            // XXX subrange typedie is useless for this, use die
            Dwarf_Die subrange_typedie = get_type_die(dbg, die);//subrange_die);

            if(subrange_typedie){
                char unused_outtype[250] = {0};
                Dwarf_Unsigned membsz = 0;
                Dwarf_Half unused_base_tag = 0;
                Dwarf_Die unused_base_die = NULL;
                Dwarf_Unsigned unused_base_data_type_offset = 0;
                Dwarf_Half membencoding = 0;
                generate_data_type_info(dbg, compile_unit, subrange_typedie,
                        unused_outtype, &membsz, &unused_base_tag,
                        &unused_base_die, &membencoding,
                        &unused_base_data_type_offset, arrmembsz,
                        arrmembencoding, classification, maxlen, level+1);

                //write_tabs(level);
                //printf("got array memb sz %#llx\n", membsz);

                *arrmembsz = membsz;
                *arrmembencoding = membencoding;
            }
            
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

                *outsize = NON_COMPILE_TIME_CONSTANT_SIZE;
                break;
            }

            // XXX don't forget to check for overflow
            char arrdim[96] = {0};
            snprintf(arrdim, sizeof(arrdim), "[%#llx]", nmemb);
            strcat(outtype, arrdim);

            *outsize *= nmemb;

            Dwarf_Die sibling_die = get_sibling_die(dbg, subrange_die);

            /* unlikely, but just to be sure */
            if(subrange_die != *base_die)
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

        if(typedie != *base_die)
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

static die_t *create_new_die(dwarfinfo_t *d, void *, Dwarf_Die, int);

static void get_die_data_type_info(dwarfinfo_t *dwarfinfo, void *compile_unit,
        die_t **die, int level){
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
    Dwarf_Half base_tag = 0, base_die_encoding = 0;
    Dwarf_Die base_die = NULL;

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

        // XXX for some reason calling dwarf_formsdata with this
        // attribute wipes die->die_databytessize...
        Dwarf_Unsigned sz = (*die)->die_databytessize;

        Dwarf_Attribute dw_at_encoding_attr = NULL;

        /* this will fail for DW_TAG_enumeration_type, who cares */
        get_die_attribute(dbg, (*die)->die_datatypedie, DW_AT_encoding,
                &dw_at_encoding_attr);

        if(dw_at_encoding_attr){
            get_form_data_from_attr(dbg, dw_at_encoding_attr,
                    &((*die)->die_datatypeencoding), FORMSDATA);
            dwarf_dealloc(dbg, dw_at_encoding_attr, DW_DLA_ATTR);
            (*die)->die_databytessize = sz;
        }
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
        Dwarf_Unsigned base_data_type_die_offset = 0;
        Dwarf_Unsigned arrmembsz = 0;
        Dwarf_Half arrmembencoding = 0;

        unsigned int classification = 0;

        generate_data_type_info(dwarfinfo->di_dbg, compile_unit,
                (*die)->die_datatypedie, name, &size, &base_tag,
                &base_die, &base_die_encoding, &base_data_type_die_offset,
                &arrmembsz, &arrmembencoding, &classification, maxlen, 0);

        if(IS_POINTER)
            classification |= DTC_POINTER;

        IS_POINTER = 0;

        (*die)->die_databytessize = size;
        (*die)->die_datatypename = strdup(name);
        (*die)->die_datatypeencoding = base_die_encoding;
        (*die)->die_basedatatypedieoffset = base_data_type_die_offset;
        (*die)->die_arrmembsz = arrmembsz;
        (*die)->die_datatypeclass = classification;
    }

    (*die)->die_basedatatypedietag = base_tag;
}

static die_t *CUR_PARENTS[1000] = {0};

static void copy_location_lists(Dwarf_Debug dbg, die_t **die,
        Dwarf_Half whichattr, int level){
    Dwarf_Attribute attr = NULL;
    get_die_attribute(dbg, (*die)->die_dwarfdie, whichattr, &attr);

    if(!attr)
        return;

    Dwarf_Error d_error = NULL;
    Dwarf_Loc_Head_c loclisthead = NULL;

    int lret = dwarf_get_loclist_c(attr, &loclisthead,
            &((*die)->die_loclistcnt), &d_error);

    dwarf_dealloc(dbg, attr, DW_DLA_ATTR);

    if(lret == DW_DLV_OK){
        Dwarf_Unsigned lcount = (*die)->die_loclistcnt;

        initialize_die_loclists(&((*die)->die_loclists), lcount);

        for(Dwarf_Unsigned i=0; i<lcount; i++){
            Dwarf_Small loclist_source = 0, lle_value = 0;
            Dwarf_Addr lopc = 0, hipc = 0;
            Dwarf_Unsigned ulocentry_count = 0, section_offset = 0,
                           locdesc_offset = 0;
            Dwarf_Locdesc_c locentry = NULL;

            /* d_error is still NULL */

            lret = dwarf_get_locdesc_entry_c(loclisthead,
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

                        uint64_t cudie_lopc = 0, cudie_hipc = 0;

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

                            cudie_lopc = cudie->die_low_pc;
                            cudie_hipc = cudie->die_high_pc;
                            //write_tabs(level);
                            //printf("Found CU DIE: '%s'\n", cudie->die_diename);

                            //lopc += cudie->die_low_pc;
                            //hipc += cudie->die_low_pc;
                        }

                        uint64_t locdesc_lopc = lopc + cudie_lopc;
                        uint64_t locdesc_hipc = hipc + cudie_lopc;

                        void *locdesc =
                            create_location_description(loclist_source,
                                    locdesc_lopc, locdesc_hipc, op, opd1,
                                    opd2, opd3, offsetforbranch);

                        //dprintf("locdesc %p for DIE '%s'\n", locdesc, (*die)->die_diename);
                        //dprintf("DIE '%s': ", (*die)->die_diename);
                        if(j > 0){
                          //  printf("j>0: adding locdesc %p\n", locdesc);
                            add_additional_location_description(whichattr,
                                    (*die)->die_loclists, locdesc, i);

                            //void *copytest = copy_locdesc((*die)->die_loclists[i]);
                            //int bkpthere = 0;
                        }
                        else{
                            if(whichattr == DW_AT_location){
                            //    printf("(*die)->die_loclists[%lld] = %p\n", i, locdesc);
                                (*die)->die_loclists[i] = locdesc;
                            }
                            else if(whichattr == DW_AT_frame_base){
                              //  printf("(*die)->die_framebaselocdesc = %p\n", locdesc);
                                (*die)->die_framebaselocdesc = locdesc;
                            }
                            else{
                                dprintf("Called with whichattr %#x\n", whichattr);
                                // XXX
                                abort();
                            }
                        }
                    }
                    else{
                        dprintf("dwarf_get_location_op_value_c: %s\n", dwarf_errmsg_by_number(dwarf_errno(d_error)));
                        dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
                    }
                }
            }
            else{
                dprintf("dwarf_get_locdesc_entry_c: %s\n", dwarf_errmsg_by_number(dwarf_errno(d_error)));
                dwarf_dealloc(dbg, d_error, DW_DLA_ERROR);
            }
        }
    }
    else if(lret == DW_DLV_ERROR){
        dprintf("dwarf_get_loclist_c: %s\n", dwarf_errmsg_by_number(dwarf_errno(d_error)));
    }

    dwarf_loc_head_c_dealloc(loclisthead);

    /* If this DIE is the child of a subroutine DIE, initialize its
     * frame base location description.
     */
    if((*die)->die_tag != DW_TAG_subprogram && level > 0){
        int pos = level;
        die_t *curparent = CUR_PARENTS[pos];

        while(pos >= 0 &&
                (!curparent || curparent->die_tag != DW_TAG_subprogram)){
            curparent = CUR_PARENTS[pos--];
        }

        if(curparent->die_tag == DW_TAG_subprogram){
            (*die)->die_framebaselocdesc =
                copy_locdesc(curparent->die_framebaselocdesc);
        }
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

    dwarf_die_abbrev_children_flag((*die)->die_dwarfdie,
            &((*die)->die_haschildren));

    get_die_data_type_info(dwarfinfo, compile_unit, die, level);

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

    return 0;
}

enum {
    PTR, STRUCT_OR_UNION, ARRAY
};

static int does_die_represent_what(die_t *die, int *retval, int what,
        sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!retval){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    if(what == PTR)
        *retval = die->die_datatypeclass & DTC_POINTER;
    else if(what == STRUCT_OR_UNION)
        *retval = die->die_datatypeclass & DTC_STRUCT_OR_UNION;
    else if(what == ARRAY)
        *retval = die->die_datatypeclass & DTC_ARRAY;

    return 0;
}

int die_represents_pointer(die_t *die, int *retval, sym_error_t *e){
    return does_die_represent_what(die, retval, PTR, e);
}

int die_represents_struct_or_union(die_t *die, int *retval, sym_error_t *e){
    return does_die_represent_what(die, retval, STRUCT_OR_UNION, e);
}

int die_represents_array(die_t *die, int *retval, sym_error_t *e){
    return does_die_represent_what(die, retval, ARRAY, e);
}

static Dwarf_Half die_has_children(Dwarf_Die die){
    Dwarf_Half result = 0;
    dwarf_die_abbrev_children_flag(die, &result);

    return result;
}

static void describe_die_internal(die_t *die, int level){
    if(!die)
        return;

    const char *varnamecolorstr = LIGHT_MAGENTA;

    if(die->die_haschildren)
        varnamecolorstr = GREEN;

    printf("%#llx: <%d> <%s>: '%s%s%s', is parent: %d",
            die->die_dieoffset, level, die->die_tagname,
            varnamecolorstr, die->die_diename, RESET,
            die->die_haschildren);

    printf(", type DIE at %s%#llx%s",
            die->die_datatypedieoffset!=0?CYAN:"",
            die->die_datatypedieoffset, die->die_datatypedieoffset!=0?RESET:"");

    if(die->die_datatypedieoffset!=0){
        printf(", type = '"LIGHT_BLUE"%s"RESET"'", die->die_datatypename);

        if(die->die_tag != DW_TAG_subprogram){
            printf(", sizeof(%s%s%s) = "LIGHT_YELLOW"%#llx"RESET"",
                    varnamecolorstr, die->die_diename, RESET, die->die_databytessize);
        }
    }

    if(die->die_tag == DW_TAG_formal_parameter ||
            die->die_tag == DW_TAG_variable ||
            die->die_tag == DW_TAG_member){
        const char *e = NULL;
        dwarf_get_ATE_name(die->die_datatypeencoding, &e);

        if(e)
            printf(", data type encoding = "WHITE_BG""BLACK"%s"RESET""RESET_BG, e);
        int c = die->die_datatypeclass;
        int ptr = c & DTC_POINTER;
        int soru = c & DTC_STRUCT_OR_UNION;
        int a = c & DTC_ARRAY;
        int none = (!ptr && !soru && !a);
        printf(", %s%sPOINTER%s%s %s%sSTRUCT OR UNION%s%s %s%sARRAY%s%s %s%sOTHER%s%s",
                ptr?GREEN_BG:RED_BG, ptr?BLACK:"", RESET, RESET_BG,
                soru?GREEN_BG:RED_BG, soru?BLACK:"", RESET, RESET_BG,
                a?GREEN_BG:RED_BG, a?BLACK:"", RESET, RESET_BG,
                none?GREEN_BG:RED_BG, none?BLACK:"", RESET, RESET_BG);
    }
   
    if(die->die_datatypedietag == DW_TAG_array_type){
        printf(", membsz = %s%s%#llx%s%s",
                MAGENTA_BG, LIGHT_YELLOW, die->die_arrmembsz,
                RESET, RESET_BG);
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

    // XXX for testing read_buffer location lists
    uint64_t pc = 0x1000191d4;

    if(die->die_loclistcnt > 0){
        putseparator = 1;

        for(Dwarf_Unsigned i=0; i<die->die_loclistcnt; i++){
            void *current = die->die_loclists[i];

            int idx2 = 0;
            while(current){
                int byteswritten = 0;
                describe_location_description(current, 0, i, idx2, level, &byteswritten);

                if(byteswritten > maxbyteswritten)
                    maxbyteswritten = byteswritten;

                putchar('\n');
                idx2++;
                current = get_next_location_description(current);
            }

            current = die->die_loclists[i];

            if(current){
                write_tabs(level);
                write_spaces(level+4);
                printf(RED"| "RESET);

                write_spaces(4);
                printf(GREEN"|"RESET"\n");

                write_tabs(level);
                write_spaces(level+4);
                printf(RED"| "RESET);
                write_spaces(4);

                printf(GREEN"---"RESET);

                uint64_t result = 0;

                char *loc_desc_decoded =
                    decode_location_description(die->die_framebaselocdesc,
                            current, pc, &result);
                printf(" Decoded: '%s'\n", loc_desc_decoded);
                free(loc_desc_decoded);
            }
        }
    }

    if(die->die_framebaselocdesc){
        int byteswritten = 0;
        describe_location_description(die->die_framebaselocdesc, 1, 0, 0, level, &byteswritten);
        if(byteswritten > maxbyteswritten)
            maxbyteswritten = byteswritten;

        putchar('\n');

        write_tabs(level);
        write_spaces(level+4);
        printf(RED"| "RESET);

        write_spaces(4);
        printf(GREEN"|"RESET"\n");

        write_tabs(level);
        write_spaces(level+4);
        printf(RED"| "RESET);
        write_spaces(4);

        printf(GREEN"---"RESET);

        uint64_t result = 0;
        char *loc_desc_decoded =
            decode_location_description(die->die_framebaselocdesc,
                    die->die_framebaselocdesc, pc, &result);
        printf(" Decoded: '%s'\n", loc_desc_decoded);
        free(loc_desc_decoded);

        putseparator = 1;
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
    if(!based_on)
        return NULL;

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
    const static Dwarf_Half accepted_tags[] = {
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

     //write_tabs(level);
     //describe_die_internal(current, level);
}

static void die_free(Dwarf_Debug dbg, die_t *die, int critical){
    if(!die)
        return;

    if(critical){
        if(die->die_dwarfdie){
            dwarf_dealloc(dbg, die->die_dwarfdie, DW_DLA_DIE);
            die->die_dwarfdie = NULL;
        }
    }
    else{
        free(die->die_children);
        die->die_children = NULL;
    }

    if(die->die_srclines){
        dwarf_srclines_dealloc(dbg, die->die_srclines, die->die_srclinescnt);
        die->die_srclines = NULL;
    }

    char *namecopy = NULL;

    if(die->die_diename)
        namecopy = strdup(die->die_diename);
    
    if(!die->die_anon && !die->die_lexblock){
        if(die->die_diename)
            dwarf_dealloc(dbg, die->die_diename, DW_DLA_STRING);
    }
    else{
        //printf("Freeing %p - '%s'\n", die->die_diename, die->die_diename);
        free(die->die_diename);
    }

    die->die_diename = NULL;

    if(die->die_datatypedie){
        dwarf_dealloc(dbg, die->die_datatypedie, DW_DLA_DIE);
        die->die_datatypedie = NULL;
    }

    /* see get_die_data_type_info */
    if(die->die_datatypedietag == DW_TAG_base_type ||
            die->die_datatypedietag == DW_TAG_enumeration_type){
        dwarf_dealloc(dbg, die->die_datatypename, DW_DLA_STRING);
    }
    else{
        free(die->die_datatypename);
    }

    for(Dwarf_Unsigned i=0; i<die->die_loclistcnt; i++){
       // printf("DIE '%s':", namecopy);
        loc_free(die->die_loclists[i]);
        //die->die_loclists[i] = NULL;
    }

    if(die->die_loclists){
        free(die->die_loclists);
        die->die_loclists = NULL;
        die->die_loclistcnt = 0;
    }

    if(die->die_framebaselocdesc){
        //printf("Freeing frame base locdesc for DIE '%s':", namecopy);
        loc_free(die->die_framebaselocdesc);
        die->die_framebaselocdesc = NULL;
    }
    free(namecopy);

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
    int critical = 0;

    if(should_add_die_to_tree(current)){
        add_die_to_tree(current, level);
        current->die_dwarfdieneedsfree = 0;
    }
    else{
        current->die_dwarfdieneedsfree = 1;
        die_free(dwarfinfo->di_dbg, current, critical);
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

        if(ret == DW_DLV_ERROR){
            dprintf("dwarf_siblingof_b level %d: %s\n", level, dwarf_errmsg_by_number(ret));
            dwarf_dealloc(dwarfinfo->di_dbg, d_error, DW_DLA_ERROR);
        }
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
            die_free(dwarfinfo->di_dbg, newdie, critical);
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
//    int critical = 1;
    if(!die)
        return;

    die_free(dbg, die, 1);

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

int die_evaluate_location_description(die_t *die, uint64_t pc,
        uint64_t *resultout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!resultout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    /* Iterate over all the location lists until we find the right one. */
    for(Dwarf_Signed i=0; i<die->die_loclistcnt; i++){
        void *current = die->die_loclists[i];

        if(current && !is_locdesc_in_bounds(current, pc))
            continue;

        char *s = decode_location_description(die->die_framebaselocdesc,
                current, pc, resultout);
        free(s);
        break;
    }

    return 0;
}

int die_get_array_elem_size(die_t *die, uint64_t *elemszout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!elemszout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    *elemszout = die->die_arrmembsz;
    return 0;
}

int die_get_data_type_str(die_t *die, char **datatypeout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!datatypeout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    if(!die->die_datatypename){
        errset(e, DIE_ERROR_KIND, DIE_NO_DATA_TYPE_NAME);
        return 1;
    }

    strncpy(*datatypeout, die->die_datatypename, strlen(die->die_datatypename));

    return 0;
}

int die_get_encoding(die_t *die, uint64_t *encodingout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!encodingout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    *encodingout = die->die_datatypeencoding;
    return 0;
}

int die_get_high_pc(die_t *die, uint64_t *highpcout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!highpcout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    *highpcout = die->die_high_pc;
    return 0;
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

int die_get_line_info_from_pc(Dwarf_Debug dbg, die_t *die, uint64_t pc,
        char **srcfilename, char **srcfunction, uint64_t *srclineno,
        sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    /* Not a compilation unit DIE */
    if(die->die_tag != DW_TAG_compile_unit){
        errset(e, DIE_ERROR_KIND, DIE_NOT_COMPILE_UNIT_DIE);
        return 1;
    }

    for(Dwarf_Signed i=0; i<die->die_srclinescnt; i++){
        Dwarf_Line line = die->die_srclines[i];

        if(pc == get_dwarf_line_virtual_addr(dbg, line)){
            char *fname = get_dwarf_line_filename(dbg, line);

            /* We are only interested in the file name */
            if(fname){
                char *slash = strrchr(fname, '/');

                if(slash)
                    *srcfilename = strdup(slash + 1);
                else
                    *srcfilename = strdup(fname);

                dwarf_dealloc(dbg, fname, DW_DLA_STRING);
            }

            *srclineno = get_dwarf_line_lineno(dbg, line);

            die_t *fxndie = NULL;
            int ret = die_search(die, (void *)pc, DIE_SEARCH_FUNCTION_BY_PC,
                    &fxndie, e);

            if(ret){
                free(*srcfilename);
                *srcfilename = NULL;
                *srclineno = 0;
                return 1;
            }

            *srcfunction = strdup(fxndie->die_diename);

            return 0;
        }
    }

    errset(e, DIE_ERROR_KIND, DIE_COULD_NOT_GET_LINE_INFO);
    return 1;
}

int die_get_low_pc(die_t *die, uint64_t *lowpcout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!lowpcout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    *lowpcout = die->die_low_pc;
    return 0;
}

int die_get_members(die_t *die, die_t *cu_root_die,
        die_t ***membersout, int *len, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!membersout || !len){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    die_t *target = die;
    Dwarf_Half tag = die->die_tag;

    if(tag != DW_TAG_structure_type && tag != DW_TAG_union_type){
        die_t *d = NULL;
        if(die_search(cu_root_die, (void *)die->die_basedatatypedieoffset,
                    DIE_SEARCH_IF_DIE_OFFSET_MATCHES, &d, e)){
            errset(e, DIE_ERROR_KIND, DIE_NOT_STRUCT_OR_UNION);
            return 1;
        }

        target = d;
    }

    die_t **members = malloc(sizeof(die_t));
    members[0] = NULL;

    int idx = 0;
    die_t *child = target->die_children[idx];

    while(child){
        if(child->die_tag == DW_TAG_member){
            die_t **members_rea = realloc(members, sizeof(die_t) * ++(*len));
            members = members_rea;
            members[(*len) - 1] = child;
        }

        child = target->die_children[++idx];
    }

    *membersout = members;

    return 0;
}

int die_get_name(die_t *die, char **dienameout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!dienameout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    *dienameout = die->die_diename;
    return 0;
}

int die_get_member_offset(die_t *die, uint64_t *offout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!offout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    *offout = die->die_memb_off;
    return 0;
}

int die_get_parameters(die_t *die, die_t ***paramsout, int *lenout,
        sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(die->die_tag != DW_TAG_subprogram){
        errset(e, DIE_ERROR_KIND, DIE_NOT_FUNCTION_DIE);
        return 1;
    }

    if(!paramsout || !lenout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    /* We don't have to recurse farther down the DIE chain,
     * parameters will be direct descendants.
     */
    die_t **params = malloc(sizeof(die_t));
    params[0] = NULL;

    int idx = 0;
    die_t *child = die->die_children[idx];

    while(child){
        if(child->die_tag == DW_TAG_formal_parameter){
            die_t **params_rea = realloc(params, sizeof(die_t) * ++(*lenout));
            params = params_rea;
            params[(*lenout) - 1] = child;
        }

        child = die->die_children[++idx];
    }

    *paramsout = params;
    return 0;
}

int die_get_parent(die_t *die, die_t **parentout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!parentout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    *parentout = die->die_parent;
    return 0;
}

int die_get_pc_of_next_line(Dwarf_Debug dbg, die_t *die,
        uint64_t start_pc, uint64_t *next_line_pc, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    /* Not a compilation unit DIE */
    if(die->die_tag != DW_TAG_compile_unit){
        errset(e, DIE_ERROR_KIND, DIE_NOT_COMPILE_UNIT_DIE);
        return 1;
    }

    if(!next_line_pc){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    uint64_t next_line = 0, start_pc_lineno = 0;

    if(die_pc_to_lineno(dbg, die, start_pc, &start_pc_lineno, e))
        return 1;

    Dwarf_Unsigned prevlineno = start_pc_lineno;

    /* Lines given back aren't guarenteed to be in chronological order. */
    for(Dwarf_Signed i=0; i<die->die_srclinescnt; i++){
        Dwarf_Line line = die->die_srclines[i];
        Dwarf_Unsigned curlineaddr = get_dwarf_line_virtual_addr(dbg, line);
        Dwarf_Unsigned curlineno = get_dwarf_line_lineno(dbg, line);

        if(curlineaddr <= start_pc || curlineno == 0 ||
                start_pc_lineno == curlineno){
            continue;
        }
        
        uint64_t current = llabs((int64_t)(next_line - start_pc));
        uint64_t diff = llabs((int64_t)(curlineaddr - start_pc));

        if(diff < current && prevlineno != curlineno)
            next_line = curlineaddr;

        prevlineno = curlineno;
    }

    /* If this is still 0, the next line's PC wasn't found. */
    if(next_line == 0){
        errset(e, DIE_ERROR_KIND, DIE_NEXT_LINE_NOT_FOUND);
        return 1;
    }

    *next_line_pc = next_line;
    return 0;
}

int die_get_pc_values_from_lineno(Dwarf_Debug dbg, die_t *die,
        uint64_t lineno, uint64_t **pcs, int *len, sym_error_t *e){
    if(!pcs || !len){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    *pcs = malloc(sizeof(uint64_t));
    (*pcs)[0] = 0;

    for(Dwarf_Signed i=0; i<die->die_srclinescnt; i++){
        Dwarf_Line line = die->die_srclines[i];
        Dwarf_Unsigned curlineaddr = get_dwarf_line_virtual_addr(dbg, line);
        Dwarf_Unsigned curlineno = get_dwarf_line_lineno(dbg, line);

        if(curlineno == lineno){
            uint64_t *pcs_rea = realloc(*pcs, sizeof(uint64_t) * ++(*len));
            *pcs = pcs_rea;
            (*pcs)[(*len) - 1] = curlineaddr;
        }
    }

    return 0;
}

int die_get_variables(Dwarf_Debug dbg, die_t *die, die_t ***vardies,
        int *len){
    if(!die || !vardies || !len)
        return 1;

    if(!(*vardies))
        *vardies = malloc(sizeof(die_t));

    if(die->die_tag == DW_TAG_variable){
        die_t **vardies_rea = realloc(*vardies, sizeof(die_t) * ++(*len));
        *vardies = vardies_rea;
        (*vardies)[(*len) - 1] = die;
    }

    if(!die->die_haschildren)
        return 0;
    else{
        int idx = 0;
        die_t *child = die->die_children[idx];

        int ret = 0;

        while(child){
            ret = die_get_variables(dbg, child, vardies, len);
            child = die->die_children[++idx];
        }

        return ret;
    }

    return 0;
}

int die_get_variable_size(die_t *die, uint64_t *sizeout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!sizeout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    *sizeout = die->die_databytessize;
    return 0;
}

int die_is_member_of_struct_or_union(die_t *die, int *retval,
        sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(!die->die_parent){
        errset(e, DIE_ERROR_KIND, DIE_NO_PARENT);
        return 1;
    }

    if(!retval){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    Dwarf_Half t = die->die_tag, tp = die->die_parent->die_tag;

    *retval = t == DW_TAG_member && (tp == DW_TAG_structure_type ||
            tp == DW_TAG_union_type);

    return 0;
}

int die_lineno_to_pc(Dwarf_Debug dbg, die_t *die, uint64_t *lineno,
        uint64_t *pcout, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(die->die_tag != DW_TAG_compile_unit){
        errset(e, DIE_ERROR_KIND, DIE_NOT_COMPILE_UNIT_DIE);
        return 1;
    }

    if(!lineno || !pcout){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

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
        if(diff == 0){
            *pcout = get_dwarf_line_virtual_addr(dbg, line);
            return 0;
        }
        else if(diff < current){
            closestlineno = curlineno;
            closestline = line;
        }
    }

    // XXX concat(outbuffer, ...
    printf("Line %lld doesn't exist, auto-adjusted to line %lld\n",
            linepassedin, closestlineno);

    *pcout = get_dwarf_line_virtual_addr(dbg, closestline);
    *lineno = closestlineno;

    return 0;
}

int die_pc_to_lineno(Dwarf_Debug dbg, die_t *die, uint64_t target_pc,
        uint64_t *lineno, sym_error_t *e){
    if(!die){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_DIE);
        return 1;
    }

    if(die->die_tag != DW_TAG_compile_unit){
        errset(e, DIE_ERROR_KIND, DIE_NOT_COMPILE_UNIT_DIE);
        return 1;
    }

    if(!lineno){
        errset(e, GENERIC_ERROR_KIND, GE_INVALID_PARAMETER);
        return 1;
    }

    /* If we're given a PC to match against, we should match exactly. */
    for(Dwarf_Signed i=0; i<die->die_srclinescnt; i++){
        Dwarf_Line line = die->die_srclines[i];
        Dwarf_Unsigned curlineaddr = get_dwarf_line_virtual_addr(dbg, line);
        //Dwarf_Unsigned curlineno = get_dwarf_line_lineno(dbg, line);

        // dprintf("line %lld @ %#llx\n", curlineno, curlineaddr);

        if(target_pc == curlineaddr){
            *lineno = get_dwarf_line_lineno(dbg, line);
            return 0;
        }
    }

    errset(e, DIE_ERROR_KIND, DIE_LINE_NOT_FOUND);
    return 1;
}

static int die_is_func_in_range(die_t *die, void *pc){
    return die->die_tag == DW_TAG_subprogram &&
        (uint64_t)pc >= die->die_low_pc && (uint64_t)pc < die->die_high_pc;
}

static int die_name_matches(die_t *die, void *name){
    return die->die_diename && strcmp(die->die_diename, (const char *)name) == 0;
}

static int die_offset_matches(die_t *die, void *offset){
    return die->die_dieoffset == (Dwarf_Unsigned)offset;
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

int die_search(die_t *start, void *data, int way, die_t **out,
        sym_error_t *e){
    int (*comparefxn)(die_t *, void *) = NULL;

    if(way == DIE_SEARCH_IF_NAME_MATCHES)
        comparefxn = die_name_matches;
    else if(way == DIE_SEARCH_FUNCTION_BY_PC)
        comparefxn = die_is_func_in_range;
    else if(way == DIE_SEARCH_IF_DIE_OFFSET_MATCHES)
        comparefxn = die_offset_matches;

    die_search_internal(start, data, comparefxn, out);

    if(!(*out)){
        errset(e, DIE_ERROR_KIND, DIE_DIE_NOT_FOUND);
        return 1;
    }
    
    return 0;
}

int initialize_and_build_die_tree_from_root_die(dwarfinfo_t *dwarfinfo,
        void *compile_unit, die_t **_root_die, sym_error_t *e){
    int is_info = 1;
    Dwarf_Error d_error = NULL;
    Dwarf_Die cu_rootdie = NULL;

    int ret = dwarf_siblingof_b(dwarfinfo->di_dbg, NULL, is_info,
            &cu_rootdie, &d_error);

    if(ret == DW_DLV_ERROR){
        errset(e, SYM_ERROR_KIND, SYM_DWARF_SIBLING_OF_B_FAILED);
        return 1;
    }

    die_t *root_die = create_new_die(dwarfinfo, compile_unit, cu_rootdie, 0);
    memset(CUR_PARENTS, 0, sizeof(CUR_PARENTS));
    CUR_PARENTS[0] = root_die;

//     if(strcmp(root_die->die_diename, "source/cmd/memcmd.c") != 0)
  //     return 0;

    construct_die_tree(dwarfinfo, compile_unit, root_die, NULL, root_die, 0);

    ret = dwarf_srclines(root_die->die_dwarfdie, &root_die->die_srclines,
            &root_die->die_srclinescnt, &d_error);
    
    if(ret == DW_DLV_ERROR){
        dwarf_dealloc(dwarfinfo->di_dbg, d_error, DW_DLA_ERROR);
        errset(e, SYM_ERROR_KIND, SYM_DWARF_SRCLINES_FAILED);
        return 1;
    }

    printf("output of display_die_tree:\n\n");

    display_die_tree_internal(root_die, 0);

    printf("end display_die_tree output\n\n");

    *_root_die = root_die;

    return 0;
}
