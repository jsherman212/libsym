#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dwarf.h>
#include <libdwarf.h>

#include "common.h"
#include "compunit.h"

#define GREEN "\033[32m"
#define RED "\033[31m"
#define CYAN "\033[36m"
#define MAGENTA "\033[35m"
#define LIGHT_MAGENTA "\033[95m"
#define LIGHT_RED "\033[91m"
#define LIGHT_YELLOW "\033[93m"
#define LIGHT_BLUE "\033[94m" // XXX purple?
#define RESET "\033[39m"

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

    /* top level data type DIE */
    Dwarf_Die die_datatypedie;
    Dwarf_Unsigned die_databytessize;
    char *die_datatypename;

    /* If this DIE represents an inlined subroutine, the following
     * two are initialized.
     */
    int die_inlinedsub;
    Dwarf_Unsigned die_aboriginoff;
};

static inline void write_tabs(int cnt){
    for(int i=0; i<cnt; i++){
        // printf("    ");
        putchar('\t');
        //putchar(' ');
        //putchar(' ');
    }
}

static int lex_block_count = 0, anon_struct_count = 0, anon_union_count = 0,
           anon_enum_count = 0;

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

#define CONST_TYPE_STR "const"
#define RESTRICT_TYPE_STR "restrict"
#define VOLATILE_TYPE_STR "volatile"
#define SUBROUTINE_TYPE_STR "(<params>)" // XXX
#define ARRAY_TYPE_STR "[]"
#define PTR_TYPE_STR "*"
#define UNKNOWN_TYPE_STR "<unknown>"

static const char *dwarf_type_tag_to_string(Dwarf_Half tag){
    /*
    const char *name = NULL;
    Dwarf_Error d_error;
    dwarf_get_TAG_name(tag, &name);

   printf("tag name '%s'\n", name);

*/
    switch(tag){
        case DW_TAG_const_type:
            return CONST_TYPE_STR; 
        case DW_TAG_restrict_type:
            return RESTRICT_TYPE_STR;
        case DW_TAG_volatile_type:
            return VOLATILE_TYPE_STR;
        case DW_TAG_subroutine_type:
            return SUBROUTINE_TYPE_STR;
        case DW_TAG_array_type:
            return ARRAY_TYPE_STR;
        case DW_TAG_pointer_type:
            return PTR_TYPE_STR;
        default:
            return UNKNOWN_TYPE_STR;
    };
}

static int should_append_tag(Dwarf_Half tag){
    return tag == DW_TAG_const_type || tag == DW_TAG_restrict_type ||
        tag == DW_TAG_volatile_type || tag == DW_TAG_subroutine_type ||
        tag == DW_TAG_array_type || tag == DW_TAG_pointer_type;
}

static Dwarf_Die get_type_die(Dwarf_Debug dbg, Dwarf_Die from){
    Dwarf_Error d_error = NULL;
    Dwarf_Attribute attr = NULL;

    dwarf_attr(from, DW_AT_type, &attr, &d_error);
    int ret = dwarf_errno(d_error);

    if(ret){
        dprintf("dwarf_attr: %s\n", dwarf_errmsg_by_number(ret));
        return NULL;
    }

    Dwarf_Unsigned offset = 0;
    dwarf_global_formref(attr, &offset, &d_error);

    ret = dwarf_errno(d_error);

    if(ret){
    //    dprintf("dwarf_offset: %s\n", dwarf_errmsg_by_number(ret));
        return NULL;
    }

    Dwarf_Die type_die = NULL;
    dwarf_offdie(dbg, offset, &type_die, &d_error);

    ret = dwarf_errno(d_error);

    if(ret){
        dprintf("dwarf_offdie: %s\n", dwarf_errmsg_by_number(ret));
        return NULL;
    }

    return type_die;
}

static void generate_data_type_info(Dwarf_Debug dbg, void *compile_unit,
        Dwarf_Die die, size_t maxlen, size_t curlen, char *outtype,
        Dwarf_Unsigned *outsize, int is_parameter, int level){
    char *type = NULL;
    Dwarf_Error d_error = NULL;
    dwarf_diename(die, &type, &d_error);

    int ret = dwarf_errno(d_error);
    if(ret){
        //strcat(outtype, "A");
        write_tabs(level);
        printf("dwarf_diename: %s (%d)\n", dwarf_errmsg_by_number(ret), ret);
        return;
    }

    d_error = NULL;

    Dwarf_Half tag;
    dwarf_tag(die, &tag, &d_error);

    ret = dwarf_errno(d_error);

    if(ret){
        write_tabs(level);
        printf("dwarf_tag: %s (%d)\n", dwarf_errmsg_by_number(ret), ret);
        return;
    }
    const char *tag_name1 = NULL;
    dwarf_get_TAG_name(tag, &tag_name1);

    Dwarf_Unsigned offset_1 = 0;
    d_error = NULL;

    dwarf_dieoffset(die, &offset_1, &d_error);

    if(tag == DW_TAG_formal_parameter){
        Dwarf_Die typedie = get_type_die(dbg, die);

        if(!typedie){
            write_tabs(level);
            printf("formal parameter typedie NULL? void?\n");
            return;
        }

        generate_data_type_info(dbg, compile_unit, typedie, maxlen, curlen,
                outtype, outsize, 1, level+1);

        return;
    }
    //printf("die name '%s' tag name '%s' offset %#llx\n", type, tag_name1, offset_1);

    /* Function pointer */
    if(tag == DW_TAG_subroutine_type){
        Dwarf_Die typedie = get_type_die(dbg, die);
        
        if(!typedie)
            strcat(outtype, "void");
        else{
            generate_data_type_info(dbg, compile_unit, typedie, maxlen, curlen,
                    outtype, outsize, 0, level+1);
        }

        strcat(outtype, "(");

        /* Get parameters */
        Dwarf_Die parameter_die = NULL;
        d_error = NULL;
        dwarf_child(die, &parameter_die, &d_error);

        ret = dwarf_errno(d_error);

        if(ret){
            write_tabs(level);
            printf("dwarf_child: %s (%d)\n", dwarf_errmsg_by_number(ret), ret);
            return;
        }

        /* No parameters */
        if(!parameter_die){
            strcat(outtype, "void)");
            return;
        }

        int dret = 0;

        while(ret != DW_DLV_NO_ENTRY){
            generate_data_type_info(dbg, compile_unit, parameter_die, maxlen, curlen,
                    outtype, outsize, 0, level+1);

            Dwarf_Die sibling_die = NULL;
            d_error = NULL;
            int is_info = 1;
            ret = dwarf_siblingof_b(dbg, parameter_die, is_info,
                    &sibling_die, &d_error);

            dret = dwarf_errno(d_error);
            if(dret){
                write_tabs(level);
                printf("dwarf_siblingof_b: %s d_error %s\n", dwarf_errmsg_by_number(ret),
                        dwarf_errmsg_by_number(dret));
            }

            parameter_die = sibling_die;

           // write_tabs(level);
            //printf("Parameter die %p\n", parameter_die);
            if(ret != DW_DLV_NO_ENTRY)
                strcat(outtype, ", ");
        }

        strcat(outtype, ")");

        return;
    }

    if(tag == DW_TAG_base_type ||
            tag == DW_TAG_enumeration_type ||
            tag == DW_TAG_structure_type ||
            tag == DW_TAG_union_type){
        if(type){
            size_t outtype_len = strlen(outtype);

            if(outtype_len + strlen(type) >= maxlen){
                write_tabs(level);
                printf("preventing buffer overflow, returning...\n");
                return;
            }

            strcat(outtype, type);
        }
        // XXX initialize outsize here, or no?
        // write_tabs(level);
        //printf("Level %d: at end of DIE chain, got type: '"RED"%s"RESET"'\n", level, type);

        //dwarf_bytesize(die, outsize, NULL);

        return;
    }

    d_error = NULL;

    Dwarf_Attribute *attrlist = NULL;
    Dwarf_Signed attrcnt = 0;

    d_error = NULL;
    ret = dwarf_attrlist(die, &attrlist, &attrcnt, &d_error);
    int dret1 = dwarf_errno(d_error);

    /*
    if(ret || dret1){
        printf("dwarf_attrlist: dret1=%s ret=%s\n", dwarf_errmsg_by_number(dret1), dwarf_errmsg_by_number(ret));
    }

    write_tabs(level);
    printf("attr count %#llx\n", attrcnt);
    */
    // XXX better way to detect this?
    if(attrcnt == 0){
        //write_tabs(level);
        //printf("before, got tag str '%s'\n", dwarf_type_tag_to_string(tag));
        char tag_str[96] = {0};

        /* It seems that if a DW_TAG_*_type has no attributes,
         * it is generic, like a void pointer. However, they can have
         * qualifiers, so we have to check for that.
         */
        if(tag == DW_TAG_pointer_type)
            strcpy(tag_str, "void *");
        else{
            strcat(tag_str, dwarf_type_tag_to_string(tag));
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
    else if(ret != DW_DLV_OK){
        write_tabs(level);
        printf("dwarf_global_formref: %s (%d)\n", dwarf_errmsg_by_number(ret), ret);
        return;
    }

    Dwarf_Die typedie = get_type_die(dbg, die);

    if(!typedie)
        return;

    // write_tabs(level);
    // printf("Level %d: got type: '"RED"%s"RESET"'\n", level, type);

    generate_data_type_info(dbg, compile_unit, typedie, maxlen, curlen,
            outtype, outsize, 0, level+1);

    if(tag == DW_TAG_array_type){
        // write_tabs(level);
        //printf("got array\n");

        /* Number of subranges denote how many dimensions */
        Dwarf_Die subrange_die = NULL;
        d_error = NULL;
        dwarf_child(die, &subrange_die, &d_error);

        ret = dwarf_errno(d_error);

        if(ret){
            write_tabs(level);
            printf("dwarf_child: %s (%d)\n", dwarf_errmsg_by_number(ret), ret);
            return;
        }

        int dret = 0;

        while(ret != DW_DLV_NO_ENTRY){
            d_error = NULL;
            Dwarf_Attribute attr = NULL;
            Dwarf_Unsigned subrange_count = 0;
            dwarf_attr(subrange_die, DW_AT_count, &attr, &d_error);
            dret = dwarf_errno(d_error);
            if(dret){
                printf("dwarf_attr: %s d_error %s\n", dwarf_errmsg_by_number(ret),
                        dwarf_errmsg_by_number(dret));
            }

            d_error = NULL;
            Dwarf_Unsigned nmemb;
            dwarf_formudata(attr, &nmemb, &d_error);
            dret = dwarf_errno(d_error);
            if(dret){
                printf("dwarf_formudata: %s d_error %s\n", dwarf_errmsg_by_number(ret),
                        dwarf_errmsg_by_number(dret));
            }

            // XXX don't forget to check for overflow
            char arrdim[96] = {0};
            snprintf(arrdim, sizeof(arrdim), "[%#llx]", nmemb);
            strcat(outtype, arrdim);

            Dwarf_Die sibling_die = NULL;
            d_error = NULL;
            int is_info = 1;
            ret = dwarf_siblingof_b(dbg, subrange_die, is_info,
                    &sibling_die, &d_error);

            dret = dwarf_errno(d_error);
            if(dret){
                write_tabs(level);
                printf("dwarf_siblingof_b: %s d_error %s\n", dwarf_errmsg_by_number(ret),
                        dwarf_errmsg_by_number(dret));
            }

            subrange_die = sibling_die;
        }

        return;
    }

    const char *type_tag_string = dwarf_type_tag_to_string(tag);
    const size_t type_tag_len = strlen(type_tag_string);

    size_t outtype_len = strlen(outtype);

    int is_typedef = tag == DW_TAG_typedef;

    /* If our current DIE is a typedef, this function will follow
     * (and append, without this check) the typedef types in the DIE chain.
     * As we return, we'll go up the DIE chain and see the "true" type
     * last. So we replace the previous typedef with the current one.
     */
    if(is_typedef){
        //write_tabs(level);
        //printf("got a typedef '%s'\n", type);
        
        Dwarf_Die typedie = get_type_die(dbg, die);

        if(!typedie){
            write_tabs(level);
            dprintf("typedie NULL? void?\n");
            return;
        }

        char *typedeftype = NULL;
        d_error = NULL;
        dwarf_diename(typedie, &typedeftype, &d_error);

        ret = dwarf_errno(d_error);

        if(ret){
            write_tabs(level);
            printf("dwarf_diename: %s\n", dwarf_errmsg_by_number(ret));
            return;
        }

        //write_tabs(level);
        //printf("got underlying typedef type '%s'\n", typedeftype);

        size_t outtypelen = strlen(outtype);
        size_t typedeftypelen = 0;

        if(typedeftype)
            typedeftypelen = strlen(typedeftype);

        long replaceat = outtypelen - typedeftypelen;

        if(replaceat < 0 || !typedeftype)
            replaceat = 0;

        /*
        write_tabs(level);
        printf("level %d: gonna replace '"RED"%s"RESET"' with '"GREEN"%s"RESET"' at outtype[%ld], outtype: '%s'\n",
                level, typedeftype, type, replaceat, outtype);
        */

        if(!typedeftype){
            strcpy(outtype, type);
            return;
        }

        if(outtypelen == 0){
            strcpy(outtype, type);
            return;
        }
        
        if(outtypelen == typedeftypelen && strcmp(outtype, typedeftype) == 0){
            strcpy(outtype, type);
            return;
        }

        long replacelen = outtypelen - typedeftypelen;

        if(replacelen < 0)
            replacelen = 0;

       // write_tabs(level);
       // printf("level %d, still here, gonna replace %ld bytes \n", level, replacelen);

        memset(&outtype[replaceat], 0, replacelen * sizeof(char));

       // write_tabs(level);
       // printf("outtype is now '%s', appending...\n", outtype);

        strcat(&outtype[replaceat], type);

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

// XXX generates the data type name (ex: const char **) and
// fetches whatever sizeof(data type) yields on the host machine
static void get_die_data_type_info(dwarfinfo_t *dwarfinfo, void *compile_unit,
        die_t **die){
    Dwarf_Error d_error;
    Dwarf_Attribute attr;
    int ret = dwarf_attr((*die)->die_dwarfdie, DW_AT_type, &attr, &d_error);

    if(ret != DW_DLV_OK){
        //        dprintf("dwarf_attr ret != DW_DLV_OK, returning\n");
        return;
    }

    dwarf_global_formref(attr, &((*die)->die_datatypedieoffset),
            &d_error);
    ret = dwarf_offdie(dwarfinfo->di_dbg, (*die)->die_datatypedieoffset,
            &((*die)->die_datatypedie), &d_error);

    if(ret != DW_DLV_OK){
        //      dprintf("no data type die, returning\n");
        return;
    }

    Dwarf_Half tag;
    dwarf_tag((*die)->die_datatypedie, &tag, &d_error);

    /* If this DIE already represents a base type (int, double, etc)
     * or an enum, we're done.
     */
    if(tag == DW_TAG_base_type || tag == DW_TAG_enumeration_type){
        dwarf_diename((*die)->die_datatypedie, &((*die)->die_datatypename),
                &d_error);
        dwarf_bytesize((*die)->die_datatypedie, &((*die)->die_databytessize),
                &d_error);
        return;
    }

    /* Otherwise, we have to follow the chain of DIEs that make up this
     * data type. For example:
     *      char **argv;
     *
     *      DW_TAG_pointer_type ->
     *          DW_TAG_pointer_type ->
     *              DW_TAG_base_type (char)
     */
    enum { maxlen = 256 };
    size_t curlen = 0;

    char name[maxlen] = {0};
    Dwarf_Unsigned size = 0;

    generate_data_type_info(dwarfinfo->di_dbg, compile_unit,
            (*die)->die_datatypedie, maxlen, curlen, name, &size, 0, 0);

    (*die)->die_databytessize = size;
    (*die)->die_datatypename = strdup(name);
}

static int copy_die_info(dwarfinfo_t *dwarfinfo, void *compile_unit,
        die_t **die){
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

    get_die_data_type_info(dwarfinfo, compile_unit, die);


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


static int cnt = 1;
static void describe_die(die_t *die, int level){
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
    if(level == 1)
        cnt++;

    if(die->die_datatypedieoffset!=0){
        printf(", type = '"LIGHT_BLUE"%s"RESET"'", die->die_datatypename);

        if(die->die_tag != DW_TAG_subprogram){
            printf(", sizeof('%s%s%s') = "LIGHT_YELLOW"%#llx"RESET"",
                    varnamecolorstr, die->die_diename, RESET, die->die_databytessize);
        }
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

static die_t *create_new_die(dwarfinfo_t *dwarfinfo, void *compile_unit,
        Dwarf_Die based_on){
    // XXX do I don't have to waste time setting stuff to NULL
    die_t *d = calloc(1, sizeof(die_t));
    d->die_dwarfdie = based_on;

    // XXX some of the stuff in this function will fail,
    // it is fine
    copy_die_info(dwarfinfo, compile_unit, &d);

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
            die_t *cd = create_new_die(dwarfinfo, compile_unit, child_die);
            construct_die_tree(dwarfinfo, compile_unit, root, parent, cd, level+1);
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

        die_t *current = create_new_die(dwarfinfo, compile_unit, cur_die);

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
        void *compile_unit, die_t **_root_die, char **error){
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

    die_t *root_die = create_new_die(dwarfinfo, compile_unit, cu_rootdie);
    memset(CUR_PARENTS, 0, sizeof(CUR_PARENTS));
    CUR_PARENTS[0] = root_die;

   //  if(strcmp(root_die->die_diename, "source/cmd/misccmd.c") != 0)
     //    return 0;
    construct_die_tree(dwarfinfo, compile_unit, root_die, NULL, root_die, 0);

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
