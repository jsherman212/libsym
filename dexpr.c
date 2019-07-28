#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dwarf.h>

#include "common.h"

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

static int is_locdesc_in_bounds(struct dwarf_locdesc *locdesc,
        uint64_t pc){
    if(!locdesc->locdesc_bounded)
        return 1;

    return pc >= locdesc->locdesc_lopc && pc < locdesc->locdesc_hipc;
}

/* #define\s+(DW_OP_\w+)\s*0x[[:xdigit:]]+ */
static const char *get_op_name(Dwarf_Small op){
    switch(op){
        case DW_OP_addr:
            return "DW_OP_addr";
        case DW_OP_deref:
            return "DW_OP_deref";
        case DW_OP_const1u:
            return "DW_OP_const1u";
        case DW_OP_const1s:
            return "DW_OP_const1s";
        case DW_OP_const2u:
            return "DW_OP_const2u";
        case DW_OP_const2s:
            return "DW_OP_const2s";
        case DW_OP_const4u:
            return "DW_OP_const4u";
        case DW_OP_const4s:
            return "DW_OP_const4s";
        case DW_OP_const8u:
            return "DW_OP_const8u";
        case DW_OP_const8s:
            return "DW_OP_const8s";
        case DW_OP_constu:
            return "DW_OP_constu";
        case DW_OP_consts:
            return "DW_OP_consts";
        case DW_OP_dup:
            return "DW_OP_dup";
        case DW_OP_drop:
            return "DW_OP_drop";
        case DW_OP_over:
            return "DW_OP_over";
        case DW_OP_pick:
            return "DW_OP_pick";
        case DW_OP_swap:
            return "DW_OP_swap";
        case DW_OP_rot:
            return "DW_OP_rot";
        case DW_OP_xderef:
            return "DW_OP_xderef";
        case DW_OP_abs:
            return "DW_OP_abs";
        case DW_OP_and:
            return "DW_OP_and";
        case DW_OP_div:
            return "DW_OP_div";
        case DW_OP_minus:
            return "DW_OP_minus";
        case DW_OP_mod:
            return "DW_OP_mod";
        case DW_OP_mul:
            return "DW_OP_mul";
        case DW_OP_neg:
            return "DW_OP_neg";
        case DW_OP_not:
            return "DW_OP_not";
        case DW_OP_or:
            return "DW_OP_or";
        case DW_OP_plus:
            return "DW_OP_plus";
        case DW_OP_plus_uconst:
            return "DW_OP_plus_uconst";
        case DW_OP_shl:
            return "DW_OP_shl";
        case DW_OP_shr:
            return "DW_OP_shr";
        case DW_OP_shra:
            return "DW_OP_shra";
        case DW_OP_xor:
            return "DW_OP_xor";
        case DW_OP_bra:
            return "DW_OP_bra";
        case DW_OP_eq:
            return "DW_OP_eq";
        case DW_OP_ge:
            return "DW_OP_ge";
        case DW_OP_gt:
            return "DW_OP_gt";
        case DW_OP_le:
            return "DW_OP_le";
        case DW_OP_lt:
            return "DW_OP_lt";
        case DW_OP_ne:
            return "DW_OP_ne";
        case DW_OP_skip:
            return "DW_OP_skip";
        case DW_OP_lit0:
            return "DW_OP_lit0";
        case DW_OP_lit1:
            return "DW_OP_lit1";
        case DW_OP_lit2:
            return "DW_OP_lit2";
        case DW_OP_lit3:
            return "DW_OP_lit3";
        case DW_OP_lit4:
            return "DW_OP_lit4";
        case DW_OP_lit5:
            return "DW_OP_lit5";
        case DW_OP_lit6:
            return "DW_OP_lit6";
        case DW_OP_lit7:
            return "DW_OP_lit7";
        case DW_OP_lit8:
            return "DW_OP_lit8";
        case DW_OP_lit9:
            return "DW_OP_lit9";
        case DW_OP_lit10:
            return "DW_OP_lit10";
        case DW_OP_lit11:
            return "DW_OP_lit11";
        case DW_OP_lit12:
            return "DW_OP_lit12";
        case DW_OP_lit13:
            return "DW_OP_lit13";
        case DW_OP_lit14:
            return "DW_OP_lit14";
        case DW_OP_lit15:
            return "DW_OP_lit15";
        case DW_OP_lit16:
            return "DW_OP_lit16";
        case DW_OP_lit17:
            return "DW_OP_lit17";
        case DW_OP_lit18:
            return "DW_OP_lit18";
        case DW_OP_lit19:
            return "DW_OP_lit19";
        case DW_OP_lit20:
            return "DW_OP_lit20";
        case DW_OP_lit21:
            return "DW_OP_lit21";
        case DW_OP_lit22:
            return "DW_OP_lit22";
        case DW_OP_lit23:
            return "DW_OP_lit23";
        case DW_OP_lit24:
            return "DW_OP_lit24";
        case DW_OP_lit25:
            return "DW_OP_lit25";
        case DW_OP_lit26:
            return "DW_OP_lit26";
        case DW_OP_lit27:
            return "DW_OP_lit27";
        case DW_OP_lit28:
            return "DW_OP_lit28";
        case DW_OP_lit29:
            return "DW_OP_lit29";
        case DW_OP_lit30:
            return "DW_OP_lit30";
        case DW_OP_lit31:
            return "DW_OP_lit31";
        case DW_OP_reg0:
            return "DW_OP_reg0";
        case DW_OP_reg1:
            return "DW_OP_reg1";
        case DW_OP_reg2:
            return "DW_OP_reg2";
        case DW_OP_reg3:
            return "DW_OP_reg3";
        case DW_OP_reg4:
            return "DW_OP_reg4";
        case DW_OP_reg5:
            return "DW_OP_reg5";
        case DW_OP_reg6:
            return "DW_OP_reg6";
        case DW_OP_reg7:
            return "DW_OP_reg7";
        case DW_OP_reg8:
            return "DW_OP_reg8";
        case DW_OP_reg9:
            return "DW_OP_reg9";
        case DW_OP_reg10:
            return "DW_OP_reg10";
        case DW_OP_reg11:
            return "DW_OP_reg11";
        case DW_OP_reg12:
            return "DW_OP_reg12";
        case DW_OP_reg13:
            return "DW_OP_reg13";
        case DW_OP_reg14:
            return "DW_OP_reg14";
        case DW_OP_reg15:
            return "DW_OP_reg15";
        case DW_OP_reg16:
            return "DW_OP_reg16";
        case DW_OP_reg17:
            return "DW_OP_reg17";
        case DW_OP_reg18:
            return "DW_OP_reg18";
        case DW_OP_reg19:
            return "DW_OP_reg19";
        case DW_OP_reg20:
            return "DW_OP_reg20";
        case DW_OP_reg21:
            return "DW_OP_reg21";
        case DW_OP_reg22:
            return "DW_OP_reg22";
        case DW_OP_reg23:
            return "DW_OP_reg23";
        case DW_OP_reg24:
            return "DW_OP_reg24";
        case DW_OP_reg25:
            return "DW_OP_reg25";
        case DW_OP_reg26:
            return "DW_OP_reg26";
        case DW_OP_reg27:
            return "DW_OP_reg27";
        case DW_OP_reg28:
            return "DW_OP_reg28";
        case DW_OP_reg29:
            return "DW_OP_reg29";
        case DW_OP_reg30:
            return "DW_OP_reg30";
        case DW_OP_reg31:
            return "DW_OP_reg31";
        case DW_OP_breg0:
            return "DW_OP_breg0";
        case DW_OP_breg1:
            return "DW_OP_breg1";
        case DW_OP_breg2:
            return "DW_OP_breg2";
        case DW_OP_breg3:
            return "DW_OP_breg3";
        case DW_OP_breg4:
            return "DW_OP_breg4";
        case DW_OP_breg5:
            return "DW_OP_breg5";
        case DW_OP_breg6:
            return "DW_OP_breg6";
        case DW_OP_breg7:
            return "DW_OP_breg7";
        case DW_OP_breg8:
            return "DW_OP_breg8";
        case DW_OP_breg9:
            return "DW_OP_breg9";
        case DW_OP_breg10:
            return "DW_OP_breg10";
        case DW_OP_breg11:
            return "DW_OP_breg11";
        case DW_OP_breg12:
            return "DW_OP_breg12";
        case DW_OP_breg13:
            return "DW_OP_breg13";
        case DW_OP_breg14:
            return "DW_OP_breg14";
        case DW_OP_breg15:
            return "DW_OP_breg15";
        case DW_OP_breg16:
            return "DW_OP_breg16";
        case DW_OP_breg17:
            return "DW_OP_breg17";
        case DW_OP_breg18:
            return "DW_OP_breg18";
        case DW_OP_breg19:
            return "DW_OP_breg19";
        case DW_OP_breg20:
            return "DW_OP_breg20";
        case DW_OP_breg21:
            return "DW_OP_breg21";
        case DW_OP_breg22:
            return "DW_OP_breg22";
        case DW_OP_breg23:
            return "DW_OP_breg23";
        case DW_OP_breg24:
            return "DW_OP_breg24";
        case DW_OP_breg25:
            return "DW_OP_breg25";
        case DW_OP_breg26:
            return "DW_OP_breg26";
        case DW_OP_breg27:
            return "DW_OP_breg27";
        case DW_OP_breg28:
            return "DW_OP_breg28";
        case DW_OP_breg29:
            return "DW_OP_breg29";
        case DW_OP_breg30:
            return "DW_OP_breg30";
        case DW_OP_breg31:
            return "DW_OP_breg31";
        case DW_OP_regx:
            return "DW_OP_regx";
        case DW_OP_fbreg:
            return "DW_OP_fbreg";
        case DW_OP_bregx:
            return "DW_OP_bregx";
        case DW_OP_piece:
            return "DW_OP_piece";
        case DW_OP_deref_size:
            return "DW_OP_deref_size";
        case DW_OP_xderef_size:
            return "DW_OP_xderef_size";
        case DW_OP_nop:
            return "DW_OP_nop";
        case DW_OP_push_object_address:
            return "DW_OP_push_object_address";
        case DW_OP_call2:
            return "DW_OP_call2";
        case DW_OP_call4:
            return "DW_OP_call4";
        case DW_OP_call_ref:
            return "DW_OP_call_ref";
        case DW_OP_form_tls_address:
            return "DW_OP_form_tls_address";
        case DW_OP_call_frame_cfa:
            return "DW_OP_call_frame_cfa";
        case DW_OP_bit_piece:
            return "DW_OP_bit_piece";
        case DW_OP_implicit_value:
            return "DW_OP_implicit_value";
        case DW_OP_stack_value:
            return "DW_OP_stack_value";
        case DW_OP_implicit_pointer:
            return "DW_OP_implicit_pointer";
        case DW_OP_addrx:
            return "DW_OP_addrx";
        case DW_OP_constx:
            return "DW_OP_constx";
        case DW_OP_entry_value:
            return "DW_OP_entry_value";
        case DW_OP_const_type:
            return "DW_OP_const_type";
        case DW_OP_regval_type:
            return "DW_OP_regval_type";
        case DW_OP_deref_type:
            return "DW_OP_deref_type";
        case DW_OP_xderef_type:
            return "DW_OP_xderef_type";
        case DW_OP_convert:
            return "DW_OP_convert";
        case DW_OP_reinterpret:
            return "DW_OP_reinterpret";
        default:
            return "<unknown expression opcode>";
    };
}

static char *get_register_name(Dwarf_Half op){
    char regstr[8] = {0};

    if(op < 29)
        snprintf(regstr, sizeof(regstr), "$x%d", op);
    else if(op == 29)
        strcat(regstr, "$fp");
    else if(op == 30)
        strcat(regstr, "$lr");
    else if(op == 31)
        strcat(regstr, "$sp");

    return strdup(regstr);
}


void add_additional_location_description(Dwarf_Half whichattr,
        struct dwarf_locdesc **locs, struct dwarf_locdesc *add,
        int idx){
    if(whichattr != DW_AT_location){
        dprintf("called with whichattr %#x\n", whichattr);
        // XXX
        abort();
    }

    struct dwarf_locdesc *current = locs[idx];

    while(current->locdesc_next)
        current = current->locdesc_next;

    current->locdesc_next = add;
}

void *create_location_description(Dwarf_Small loclist_source,
        uint64_t locdesc_lopc, uint64_t locdesc_hipc,
        Dwarf_Small op, Dwarf_Unsigned opd1,
        Dwarf_Unsigned opd2, Dwarf_Unsigned opd3, Dwarf_Unsigned offsetforbranch){
    struct dwarf_locdesc *locdesc = calloc(1, sizeof(struct dwarf_locdesc));

    if(loclist_source == LOCATION_LIST_ENTRY){
        locdesc->locdesc_bounded = 1;

        locdesc->locdesc_lopc = locdesc_lopc;
        locdesc->locdesc_hipc = locdesc_hipc;
    }

    locdesc->locdesc_op = op;
    locdesc->locdesc_opd1 = opd1;
    locdesc->locdesc_opd2 = opd2;
    locdesc->locdesc_opd3 = opd3;
    locdesc->locdesc_offsetforbranch = offsetforbranch;

    return locdesc;
}

// XXX returns the register DW_AT_frame_base represents
static char *evaluate_frame_base(struct dwarf_locdesc *framebaselocdesc){
    Dwarf_Small op = framebaselocdesc->locdesc_op;

    char result[8] = {0};

    switch(op){
        case DW_OP_reg0...DW_OP_reg31:
            {
                return get_register_name(op - DW_OP_reg0);
            }
        case DW_OP_regx:
            {
                Dwarf_Unsigned opd1 = framebaselocdesc->locdesc_opd1;
                char *regname = get_register_name(opd1);
                return regname;
            }
        default:
            return NULL;
    };

}

// XXX with the help of my expression evaluator, this will evaluate DWARF
// location descriptions
// XXX TODO when added inside iosdbg, this will not create a string for my expression
// evaluator, will return a computed location
char *decode_location_description(struct dwarf_locdesc *framebaselocdesc,
        struct dwarf_locdesc *locdesc, uint64_t pc){
    if(!locdesc)
        return strdup("error: NULL locdesc");

    if(!is_locdesc_in_bounds(locdesc, pc))
        return strdup("error: PC out of bounds");

    char exprstr[1024] = {0};
    uintptr_t stack[128] = {0};
    unsigned int stackptr = 0;

    struct dwarf_locdesc *ld = locdesc;

    while(ld){
        Dwarf_Small op = ld->locdesc_op;
        Dwarf_Unsigned opd1 = ld->locdesc_opd1,
                       opd2 = ld->locdesc_opd2,
                       opd3 = ld->locdesc_opd3;
        //dprintf("op '%s'\n", get_op_name(ld->locdesc_op));

        /*
        char opbuf[64] = {0};
        size_t opbuf_len_before_append = strlen(opbuf);
        snprintf(opbuf, sizeof(opbuf), " %s ", get_op_name(ld->locdesc_op));
        strcat(exprstr, opbuf);
        */
        char operatorbuf[64] = {0};
        char operandbuf[64] = {0};

        switch(op){
            case DW_OP_addr:
                {
                    //snprintf(operatorbuf, sizeof(operatorbuf), "%s", get_op_name(ld->locdesc_op));
                    snprintf(operandbuf, sizeof(operandbuf), "%#llx", opd1);

                    // XXX push this value onto the stack
                    stack[++stackptr] = opd1;

                    break;
                }
            case DW_OP_deref:
                {
                    snprintf(operatorbuf, sizeof(operatorbuf), " %s", get_op_name(ld->locdesc_op));

                    // XXX pop top of stack, read memory at that location,
                    // and then push that value onto the stack

                    break;
                }
            case DW_OP_const1u:
                {
                    break;
                }
            case DW_OP_const1s:
                {
                    break;
                }
            case DW_OP_const2u:
                {
                    break;
                }
            case DW_OP_const2s:
                {
                    break;
                }
            case DW_OP_const4u:
                {
                    break;
                }
            case DW_OP_const4s:
                {
                    break;
                }
            case DW_OP_const8u:
                {
                    break;
                }
            case DW_OP_const8s:
                {
                    break;
                }
            case DW_OP_constu:
                {
                    break;
                }
            case DW_OP_consts:
                {
                    break;
                }
            case DW_OP_dup:
                {
                    break;
                }
            case DW_OP_drop:
                {
                    break;
                }
            case DW_OP_over:
                {
                    break;
                }
            case DW_OP_pick:
                {
                    break;
                }
            case DW_OP_swap:
                {
                    break;
                }
            case DW_OP_rot:
                {
                    break;
                }
            case DW_OP_xderef:
                {
                    break;
                }
            case DW_OP_abs:
                {
                    break;
                }
            case DW_OP_and:
                {
                    break;
                }
            case DW_OP_div:
                {
                    break;
                }
            case DW_OP_minus:
                {
                    break;
                }
            case DW_OP_mod:
                {
                    break;
                }
            case DW_OP_mul:
                {
                    break;
                }
            case DW_OP_neg:
                {
                    break;
                }
            case DW_OP_not:
                {
                    break;
                }
            case DW_OP_or:
                {
                    break;
                }
            case DW_OP_plus:
                {
                    break;
                }
            case DW_OP_plus_uconst:
                {
                    break;
                }
            case DW_OP_shl:
                {
                    break;
                }
            case DW_OP_shr:
                {
                    break;
                }
            case DW_OP_shra:
                {
                    break;
                }
            case DW_OP_xor:
                {
                    break;
                }
            case DW_OP_bra:
                {
                    break;
                }
            case DW_OP_eq:
                {
                    break;
                }
            case DW_OP_ge:
                {
                    break;
                }
            case DW_OP_gt:
                {
                    break;
                }
            case DW_OP_le:
                {
                    break;
                }
            case DW_OP_lt:
                {
                    break;
                }
            case DW_OP_ne:
                {
                    break;
                }
            case DW_OP_skip:
                {
                    break;
                }
            case DW_OP_lit0...DW_OP_lit31:
                {
                    stack[++stackptr] = op - DW_OP_lit0;
                    break;
                }
            case DW_OP_reg0...DW_OP_reg31:
                {
                    // XXX will have to encounter this to implement it correctly.
                    // The DWARF standard says this and DW_OP_regx represent
                    // a "register location", which isn't the same as the value
                    // inside the register... so I guess that means the register name?
                    //
                    // LLDB reads the value of the register and pushes that
                    // onto the stack. so I guess I'll do that
                    char *regname = get_register_name(op - DW_OP_reg0);
                    snprintf(operatorbuf, sizeof(operatorbuf), "%s", regname);
                    free(regname);

                    // XXX call eval_expr on this string, then push result 
                    // onto the stack

                    break;
                }
            case DW_OP_breg0...DW_OP_breg31:
                {
                    char *regname = get_register_name(op - DW_OP_breg0);
                    snprintf(operatorbuf, sizeof(operatorbuf), "%s", regname);
                    free(regname);

                    snprintf(operandbuf, sizeof(operandbuf), "%+lld", (Dwarf_Signed)opd1);

                    // XXX concat these two strings, call eval_expr,
                    // read memory at the resulting location, and
                    // then push that value onto the stack

                    break;
                }
            case DW_OP_regx:
                {
                    char *regname = get_register_name(opd1);
                    snprintf(operatorbuf, sizeof(operatorbuf), "%s", regname);
                    free(regname);

                    // XXX call eval_expr on this string, then push result 
                    // onto the stack


                    break;
                }
            case DW_OP_fbreg:
                {
                    memset(operatorbuf, 0, sizeof(operatorbuf));

                    /* Fetch what fbreg actually is. */
                    char *fbregexpr =
                        evaluate_frame_base(framebaselocdesc);
                        //decode_location_description(framebaselocdesc, framebaselocdesc, pc);

                    snprintf(operatorbuf, sizeof(operatorbuf), "%s", fbregexpr);

                    free(fbregexpr);
                    //dprintf("exprstr '%s' fbregexpr '%s'\n", exprstr, fbregexpr);
                    //exit(0);

                    snprintf(operandbuf, sizeof(operandbuf), "%+lld", opd1);

                    // XXX concat these two strings, call eval_expr,
                    // read memory at the resulting location, and
                    // then push that value onto the stack

                    break;
                }
            case DW_OP_bregx:
                {
                    char *regname = get_register_name(opd1);
                    snprintf(operatorbuf, sizeof(operatorbuf), "%s", regname);
                    free(regname);
                    
                    snprintf(operandbuf, sizeof(operandbuf), "%+lld", (Dwarf_Signed)opd2);

                    // XXX concat these two strings, call eval_expr,
                    // read memory at the resulting location, and
                    // then push that value onto the stack

                    break;
                }
            case DW_OP_piece:
                {
                    break;
                }
            case DW_OP_deref_size:
                {
                    break;
                }
            case DW_OP_xderef_size:
                {
                    break;
                }
            case DW_OP_nop:
                {
                    /* do nothing */
                    break;
                }
            case DW_OP_push_object_address:
                {
                    break;
                }
            case DW_OP_call2:
                {
                    break;
                }
            case DW_OP_call4:
                {
                    break;
                }
            case DW_OP_call_ref:
                {
                    break;
                }
            case DW_OP_form_tls_address:
                {
                    break;
                }
            case DW_OP_call_frame_cfa:
                {
                    break;
                }
            case DW_OP_bit_piece:
                {
                    break;
                }
            case DW_OP_implicit_value:
                {
                    break;
                }
            case DW_OP_stack_value:
                {
                    break;
                }
            case DW_OP_implicit_pointer:
                {
                    break;
                }
            case DW_OP_addrx:
                {
                    break;
                }
            case DW_OP_constx:
                {
                    break;
                }
            case DW_OP_entry_value:
                {
                    break;
                }
            case DW_OP_const_type:
                {
                    break;
                }
            case DW_OP_regval_type:
                {
                    break;
                }
            case DW_OP_deref_type:
                {
                    break;
                }
            case DW_OP_xderef_type:
                {
                    break;
                }
            case DW_OP_convert:
                {
                    break;
                }
            case DW_OP_reinterpret:
                {
                    break;
                }
            default:
                dprintf("Unhandled op %#x\n", op);
                abort();
        };

        strcat(exprstr, operatorbuf);
        strcat(exprstr, operandbuf);
        //strcat(exprstr, " ");

        ld = ld->locdesc_next;
    }

    return strdup(exprstr);
}

void describe_location_description(struct dwarf_locdesc *locdesc,
        int is_fb, int idx, int idx2, int level, int *byteswritten){
    write_tabs(level);
    write_spaces(level+4);

    int add = 0;

    if(is_fb){
        printf(RED"|"RESET" DW_AT_frame_base:%n", &add);
        *byteswritten += (add - strlen(RED) - strlen(RESET));
    }
    else{
        printf(RED"|"RESET" DW_AT_location ["GREEN"%d"RESET"]["RED"%d"RESET"]:%n", idx, idx2, &add);
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

void *get_next_location_description(struct dwarf_locdesc *ld){
    if(!ld)
        return NULL;

    return ld->locdesc_next;
}

void initialize_die_loclists(struct dwarf_locdesc ***locdescs,
        Dwarf_Unsigned lcount){
    *locdescs = calloc(lcount, sizeof(struct dwarf_locdesc));
}
