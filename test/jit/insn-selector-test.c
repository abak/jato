/*
 * Copyright (C) 2006  Pekka Enberg
 */

#include <libharness.h>
#include <jit/basic-block.h>
#include <jit/expression.h>
#include <jit/instruction.h>
#include <jit/insn-selector.h>
#include <jit/statement.h>
#include <jit/jit-compiler.h>

#include "vm-utils.h"

static void assert_insn(enum insn_type insn_type, struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
}

static void assert_membase_reg_insn(enum insn_type insn_type,
				    enum machine_reg src_base_reg,
				    long src_displacement,
				    enum machine_reg dest_reg,
				    struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_int_equals(src_base_reg, insn->src.base_reg->reg);
	assert_int_equals(src_displacement, insn->src.disp);
	assert_int_equals(dest_reg, insn->dest.reg->reg);
}

static void assert_memindex_reg_insn(enum insn_type insn_type,
				     enum machine_reg src_base_reg,
				     enum machine_reg src_index_reg,
				     unsigned char src_shift,
				     enum machine_reg dest_reg,
				     struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_int_equals(src_base_reg, insn->src.base_reg->reg);
	assert_int_equals(src_index_reg, insn->src.index_reg->reg);
	assert_int_equals(src_shift, insn->src.shift);
	assert_int_equals(dest_reg, insn->dest.reg->reg);
}

static void assert_reg_membase_insn(enum insn_type insn_type,
				    enum machine_reg src_reg,
				    enum machine_reg dest_reg,
				    long dest_displacement,
				    struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_int_equals(src_reg, insn->src.reg->reg);
	assert_int_equals(dest_reg, insn->dest.base_reg->reg);
	assert_int_equals(dest_displacement, insn->dest.disp);
}

static void assert_reg_memindex_insn(enum insn_type insn_type,
				     enum machine_reg src_reg,
				     enum machine_reg base_reg,
				     enum machine_reg index_reg,
				     unsigned char shift,
				     struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_int_equals(src_reg, insn->src.reg->reg);
	assert_int_equals(base_reg, insn->dest.base_reg->reg);
	assert_int_equals(index_reg, insn->dest.index_reg->reg);
	assert_int_equals(shift, insn->dest.shift);
}

static void assert_reg_insn(enum insn_type insn_type,
			    enum machine_reg expected_reg, struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_int_equals(expected_reg, insn->operand.reg->reg);
}

static void assert_reg_reg_insn(enum insn_type insn_type,
				enum machine_reg expected_src,
				enum machine_reg expected_dest,
				struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_int_equals(expected_src, insn->src.reg->reg);
	assert_int_equals(expected_dest, insn->dest.reg->reg);
}

static void assert_imm_insn(enum insn_type insn_type,
			    unsigned long expected_imm, struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_int_equals(expected_imm, insn->operand.imm);
}

static void assert_imm_reg_insn(enum insn_type insn_type,
				unsigned long expected_imm,
				enum machine_reg expected_reg,
				struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_int_equals(expected_imm, insn->src.imm);
	assert_int_equals(expected_reg, insn->dest.reg->reg);
}

static void assert_imm_membase_insn(enum insn_type insn_type,
				    unsigned long expected_imm,
				    enum machine_reg expected_base_reg,
				    long expected_disp,
				    struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_int_equals(expected_imm, insn->src.imm);
	assert_int_equals(expected_base_reg, insn->dest.base_reg->reg);
	assert_int_equals(expected_disp, insn->dest.disp);
}

static void assert_rel_insn(enum insn_type insn_type,
			    unsigned long expected_imm, struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_int_equals(expected_imm, insn->operand.rel);
}

static void assert_branch_insn(enum insn_type insn_type,
			       struct basic_block *if_true, struct insn *insn)
{
	assert_int_equals(insn_type, insn->type);
	assert_ptr_equals(if_true, insn->operand.branch_target);
}

void test_should_select_insn_for_every_statement(void)
{
	struct insn *insn;
	struct expression *expr1, *expr2;
	struct statement *stmt1, *stmt2;
	struct basic_block *bb;
	struct methodblock method = {
		.args_count = 4,
	};
	struct compilation_unit *cu;

	cu = alloc_compilation_unit(&method);
	bb = get_basic_block(cu, 0, 1);

	expr1 = binop_expr(J_INT, OP_ADD, local_expr(J_INT, 0), local_expr(J_INT, 1));

	stmt1 = alloc_statement(STMT_EXPRESSION);
	stmt1->expression = &expr1->node;

	expr2 = binop_expr(J_INT, OP_ADD, local_expr(J_INT, 2), local_expr(J_INT, 3));

	stmt2 = alloc_statement(STMT_RETURN);
	stmt2->return_value = &expr2->node;

	bb_add_stmt(bb, stmt1);
	bb_add_stmt(bb, stmt2);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 8, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_ADD_MEMBASE_REG, REG_EBP, 12, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 16, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_ADD_MEMBASE_REG, REG_EBP, 20, REG_EAX, insn);

	free_compilation_unit(cu);
}

static struct basic_block *create_local_local_binop_bb(enum binary_operator expr_op)
{
	static struct methodblock method = {
		.args_count = 2,
	};
	struct compilation_unit *cu;
	struct expression *expr;
	struct basic_block *bb;
	struct statement *stmt;

	expr = binop_expr(J_INT, expr_op, local_expr(J_INT, 0), local_expr(J_INT, 1));
	stmt = alloc_statement(STMT_RETURN);
	stmt->return_value = &expr->node;

	cu = alloc_compilation_unit(&method);
	bb = get_basic_block(cu, 0, 1);
	bb_add_stmt(bb, stmt);
	return bb;
}

static void assert_select_local_local_binop(enum binary_operator expr_op, enum insn_type insn_type)
{
	struct basic_block *bb;
	struct insn *insn;

	bb = create_local_local_binop_bb(expr_op);
	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 8, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(insn_type, REG_EBP, 12, REG_EAX, insn);

	free_compilation_unit(bb->b_parent);
}

static struct basic_block *create_local_value_binop_bb(enum binary_operator expr_op)
{
	static struct methodblock method = {
		.args_count = 2,
	};
	struct compilation_unit *cu;
	struct expression *expr;
	struct basic_block *bb;
	struct statement *stmt;

	expr = binop_expr(J_INT, expr_op, local_expr(J_INT, 0), value_expr(J_INT, 0xdeadbeef));
	stmt = alloc_statement(STMT_RETURN);
	stmt->return_value = &expr->node;

	cu = alloc_compilation_unit(&method);
	bb = get_basic_block(cu, 0, 1);
	bb_add_stmt(bb, stmt);
	return bb;
}

static void assert_select_local_value_binop(enum binary_operator expr_op, enum insn_type insn_type)
{
	struct basic_block *bb;
	struct insn *insn;

	bb = create_local_value_binop_bb(expr_op);
	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 8, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(insn_type, 0xdeadbeef, REG_EAX, insn);

	free_compilation_unit(bb->b_parent);
}

void test_select_add_local_to_local(void)
{
	assert_select_local_local_binop(OP_ADD, INSN_ADD_MEMBASE_REG);
	assert_select_local_value_binop(OP_ADD, INSN_ADD_IMM_REG);
}

void test_select_sub_local_from_local(void)
{
	assert_select_local_local_binop(OP_SUB, INSN_SUB_MEMBASE_REG);
}

void test_select_mul_local_from_local(void)
{
	assert_select_local_local_binop(OP_MUL, INSN_MUL_MEMBASE_REG);
}

void test_select_local_local_div(void)
{
	struct basic_block *bb;
	struct insn *insn;

	bb = create_local_local_binop_bb(OP_DIV);
	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 8, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_insn(INSN_CLTD, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_DIV_MEMBASE_REG, REG_EBP, 12, REG_EAX, insn);

	free_compilation_unit(bb->b_parent);
}

void test_select_local_local_rem(void)
{
	struct basic_block *bb;
	struct insn *insn;

	bb = create_local_local_binop_bb(OP_REM);
	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 8, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_insn(INSN_CLTD, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_DIV_MEMBASE_REG, REG_EBP, 12, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_reg_reg_insn(INSN_MOV_REG_REG, REG_EDX, REG_EAX, insn);

	free_compilation_unit(bb->b_parent);
}

static struct basic_block *create_unop_bb(enum unary_operator expr_op)
{
	static struct methodblock method = {
		.args_count = 1,
	};
	struct compilation_unit *cu;
	struct expression *expr;
	struct basic_block *bb;
	struct statement *stmt;

	expr = unary_op_expr(J_INT, expr_op, local_expr(J_INT, 0));
	stmt = alloc_statement(STMT_RETURN);
	stmt->return_value = &expr->node;

	cu = alloc_compilation_unit(&method);
	bb = get_basic_block(cu, 0, 1);
	bb_add_stmt(bb, stmt);
	return bb;
}

static void assert_select_local_unop(enum unary_operator expr_op, enum insn_type insn_type)
{
	struct basic_block *bb;
	struct insn *insn;

	bb = create_unop_bb(expr_op);
	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 8, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_reg_insn(insn_type, REG_EAX, insn);

	free_compilation_unit(bb->b_parent);
}

void test_select_neg_local(void)
{
	assert_select_local_unop(OP_NEG, INSN_NEG_REG);
}

static void assert_select_local_local_shift(enum binary_operator expr_op, enum insn_type insn_type)
{
	struct basic_block *bb;
	struct insn *insn;

	bb = create_local_local_binop_bb(expr_op);
	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 8, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 12, REG_ECX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_reg_reg_insn(insn_type, REG_ECX, REG_EAX, insn);

	free_compilation_unit(bb->b_parent);
}

void test_select_shl_local_to_local(void)
{
	assert_select_local_local_shift(OP_SHL, INSN_SHL_REG_REG);
}

void test_select_shr_local_to_local(void)
{
	assert_select_local_local_shift(OP_SHR, INSN_SAR_REG_REG);
}

void test_select_ushr_local_to_local(void)
{
	assert_select_local_local_shift(OP_USHR, INSN_SHR_REG_REG);
}

void test_select_or_local_from_local(void)
{
	assert_select_local_local_binop(OP_OR, INSN_OR_MEMBASE_REG);
}

void test_select_and_local_from_local(void)
{
	assert_select_local_local_binop(OP_AND, INSN_AND_MEMBASE_REG);
}

void test_select_xor_local_from_local(void)
{
	assert_select_local_local_binop(OP_XOR, INSN_XOR_MEMBASE_REG);
}

void test_select_return(void)
{
	struct compilation_unit *cu;
	struct expression *value;
	struct basic_block *bb;
	struct statement *stmt;
	struct insn *insn;

	value = value_expr(J_INT, 0xdeadbeef);

	stmt = alloc_statement(STMT_RETURN);
	stmt->return_value = &value->node;

	cu = alloc_compilation_unit(NULL);
	bb = get_basic_block(cu, 0, 1);
	bb_add_stmt(bb, stmt);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_MOV_IMM_REG, 0xdeadbeef, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_branch_insn(INSN_JMP_BRANCH, cu->exit_bb, insn);

	free_compilation_unit(cu);
}

void test_select_void_return(void)
{
	struct compilation_unit *cu;
	struct basic_block *bb;
	struct statement *stmt;
	struct insn *insn;

	stmt = alloc_statement(STMT_VOID_RETURN);

	cu = alloc_compilation_unit(NULL);
	bb = get_basic_block(cu, 0, 1);
	bb_add_stmt(bb, stmt);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_branch_insn(INSN_JMP_BRANCH, cu->exit_bb, insn);

	free_compilation_unit(cu);
}

void test_select_invoke_without_arguments(void)
{
	struct expression *expr, *args_list;
	struct basic_block *bb;
	struct statement *stmt;
	struct insn *insn;
	struct methodblock mb = {
		.args_count = 0
	};

	jit_prepare_for_exec(&mb);

	bb = get_basic_block(mb.compilation_unit, 0, 1);

	args_list = no_args_expr();
	expr = __invoke_expr(J_INT, &mb);
	expr->args_list = &args_list->node;

	stmt = alloc_statement(STMT_EXPRESSION);
	stmt->expression = &expr->node;
	bb_add_stmt(bb, stmt);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_rel_insn(INSN_CALL_REL, (unsigned long) trampoline_ptr(&mb), insn);

	free_jit_trampoline(mb.trampoline);
	free_compilation_unit(mb.compilation_unit);
}

void test_select_invoke_with_arguments(void)
{
	struct expression *invoke_expression, *args_list_expression;
	struct statement *stmt;
	struct basic_block *bb;
	struct insn *insn;
	struct methodblock mb = {
		.args_count = 2
	};

	jit_prepare_for_exec(&mb);
	bb = get_basic_block(mb.compilation_unit, 0, 1);

	args_list_expression = args_list_expr(arg_expr(value_expr(J_INT, 0x02)),
					      arg_expr(value_expr
						       (J_INT, 0x01)));
	invoke_expression = __invoke_expr(J_INT, &mb);
	invoke_expression->args_list = &args_list_expression->node;

	stmt = alloc_statement(STMT_EXPRESSION);
	stmt->expression = &invoke_expression->node;
	bb_add_stmt(bb, stmt);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_imm_insn(INSN_PUSH_IMM, 0x02, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_insn(INSN_PUSH_IMM, 0x01, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_rel_insn(INSN_CALL_REL, (unsigned long) trampoline_ptr(&mb), insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_ADD_IMM_REG, 8, REG_ESP, insn);

	free_jit_trampoline(mb.trampoline);
	free_compilation_unit(mb.compilation_unit);
}

void test_select_method_return_value_passed_as_argument(void)
{
	struct expression *no_args, *arg, *invoke, *nested_invoke;
	struct basic_block *bb;
	struct statement *stmt;
	struct insn *insn;
	struct methodblock mb = {
		.args_count = 1
	};
	struct methodblock nested_mb = {
		.args_count = 0
	};

	jit_prepare_for_exec(&mb);
	bb = get_basic_block(mb.compilation_unit, 0, 1);

	jit_prepare_for_exec(&nested_mb);

	no_args = no_args_expr();
	nested_invoke = __invoke_expr(J_INT, &nested_mb);
	nested_invoke->args_list = &no_args->node;

	arg = arg_expr(nested_invoke);
	invoke = __invoke_expr(J_INT, &mb);
	invoke->args_list = &arg->node;

	stmt = alloc_statement(STMT_EXPRESSION);
	stmt->expression = &invoke->node;
	bb_add_stmt(bb, stmt);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_rel_insn(INSN_CALL_REL, (unsigned long) trampoline_ptr(&nested_mb), insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_reg_insn(INSN_PUSH_REG, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_rel_insn(INSN_CALL_REL, (unsigned long) trampoline_ptr(&mb), insn);

	free_jit_trampoline(mb.trampoline);
	free_compilation_unit(mb.compilation_unit);

	free_jit_trampoline(nested_mb.trampoline);
	free_compilation_unit(nested_mb.compilation_unit);
}

void test_select_invokevirtual_with_arguments(void)
{
	struct expression *invoke_expr, *args;
	struct compilation_unit *cu;
	unsigned long method_index;
	unsigned long objectref;
	struct statement *stmt;
	struct basic_block *bb;
	struct insn *insn;

	objectref = 0xdeadbeef;
	method_index = 0xcafe;

	args = arg_expr(value_expr(J_REFERENCE, objectref));
	invoke_expr = __invokevirtual_expr(J_VOID, method_index);
	invoke_expr->args_list = &args->node;

	stmt = alloc_statement(STMT_EXPRESSION);
	stmt->expression = &invoke_expr->node;

	cu = alloc_compilation_unit(NULL);
	bb = get_basic_block(cu, 0, 1);
	bb_add_stmt(bb, stmt);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_imm_insn(INSN_PUSH_IMM, objectref, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_ESP, 0, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EAX, offsetof(struct object, class), REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_ADD_IMM_REG, sizeof(struct object), REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EAX,
				offsetof(struct classblock, method_table),
				REG_EAX, insn);
	
	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EAX, method_index * sizeof(void *), REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EAX, offsetof(struct methodblock, trampoline), REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EAX, offsetof(struct buffer, buf), REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_reg_insn(INSN_CALL_REG, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_ADD_IMM_REG, 4, REG_ESP, insn);

	free_compilation_unit(cu);
}

static struct statement *add_if_stmt(struct expression *expr, struct basic_block *bb, struct basic_block *true_bb)
{
	struct statement *stmt;

	stmt = alloc_statement(STMT_IF);
	stmt->expression = &expr->node;
	stmt->if_true = true_bb;
	bb_add_stmt(bb, stmt);

	return stmt;
}

static void assert_select_if_statement_local_local(enum insn_type expected,
						   enum binary_operator binop)
{
	struct basic_block *bb, *true_bb;
	struct compilation_unit *cu;
	struct expression *expr;
	struct statement *stmt;
	struct insn *insn;
	struct methodblock method = {
		.args_count = 2,
	};

	cu = alloc_compilation_unit(&method);
	bb = get_basic_block(cu, 0, 1);
	true_bb = get_basic_block(cu, 1, 2);

	expr = binop_expr(J_INT, binop, local_expr(J_INT, 0), local_expr(J_INT, 1));
	stmt = add_if_stmt(expr, bb, true_bb);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 8, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_CMP_MEMBASE_REG, REG_EBP, 12, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_branch_insn(expected, stmt->if_true, insn);

	free_compilation_unit(cu);
}

static void assert_select_if_statement_local_value(enum insn_type expected,
						   enum binary_operator binop)
{
	struct basic_block *bb, *true_bb;
	struct compilation_unit *cu;
	struct expression *expr;
	struct statement *stmt;
	struct insn *insn;
	struct methodblock method = {
		.args_count = 2,
	};

	cu = alloc_compilation_unit(&method);
	bb = get_basic_block(cu, 0, 1);
	true_bb = get_basic_block(cu, 1, 2);

	expr = binop_expr(J_INT, binop, local_expr(J_INT, 0), value_expr(J_INT, 0xcafebabe));
	stmt = add_if_stmt(expr, bb, true_bb);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, 8, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_CMP_IMM_REG, 0xcafebabe, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_branch_insn(expected, stmt->if_true, insn);

	free_compilation_unit(cu);
}

static void assert_select_if_statement(enum insn_type expected,
				       enum binary_operator binop)
{
	assert_select_if_statement_local_local(expected, binop);
	assert_select_if_statement_local_value(expected, binop);
}

void test_select_if_statement(void)
{
	assert_select_if_statement(INSN_JE_BRANCH, OP_EQ);
	assert_select_if_statement(INSN_JNE_BRANCH, OP_NE);
}

void test_select_load_class_field(void)
{
	struct compilation_unit *cu;
	struct fieldblock field;
	struct expression *expr;
	struct basic_block *bb;
	struct statement *stmt;
	struct insn *insn;
	long expected_disp;

	cu = alloc_compilation_unit(NULL);
	bb = get_basic_block(cu, 0, 1);

	expr = class_field_expr(J_INT, &field);
	stmt = alloc_statement(STMT_EXPRESSION);
	stmt->expression = &expr->node;
	bb_add_stmt(bb, stmt);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_MOV_IMM_REG, (unsigned long) &field, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	expected_disp = offsetof(struct fieldblock, static_value);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EAX, expected_disp, REG_EAX, insn);

	free_compilation_unit(cu);
}

void test_select_load_instance_field(void)
{
	struct methodblock method = {
		.args_count = 0,
	};
	struct compilation_unit *cu;
	struct expression *objectref;
	struct fieldblock field;
	struct expression *expr;
	struct statement *stmt;
	struct basic_block *bb;
	struct insn *insn;

	field.offset = 8;

	objectref = local_expr(J_REFERENCE, 0);
	expr = instance_field_expr(J_INT, &field, objectref);
	stmt = alloc_statement(STMT_EXPRESSION);
	stmt->expression = &expr->node;

	cu = alloc_compilation_unit(&method);
	bb = get_basic_block(cu, 0, 1);
	bb_add_stmt(bb, stmt);
	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, -4, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_ADD_IMM_REG, sizeof(struct object), REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_MOV_IMM_REG, field.offset, REG_EDX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_memindex_reg_insn(INSN_MOV_MEMINDEX_REG, REG_EAX, REG_EDX, 2, REG_EAX, insn);

	free_compilation_unit(cu);
}

void test_store_value_to_class_field(void)
{
	struct expression *store_target;
	struct expression *store_value;
	struct compilation_unit *cu;
	struct fieldblock field;
	struct basic_block *bb;
	struct statement *stmt;
	long expected_disp;
	struct insn *insn;

	cu = alloc_compilation_unit(NULL);
	bb = get_basic_block(cu, 0, 1);
	store_target = class_field_expr(J_INT, &field);
	store_value  = value_expr(J_INT, 0xcafebabe);
	stmt = alloc_statement(STMT_STORE);
	stmt->store_dest = &store_target->node;
	stmt->store_src  = &store_value->node;
	bb_add_stmt(bb, stmt);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_MOV_IMM_REG, (unsigned long) &field, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	expected_disp = offsetof(struct fieldblock, static_value);
	assert_imm_membase_insn(INSN_MOV_IMM_MEMBASE, 0xcafebabe, REG_EAX, expected_disp, insn);

	free_compilation_unit(cu);
}

void test_store_value_to_instance_field(void)
{
	struct methodblock method = {
		.args_count = 0,
	};
	struct expression *store_target;
	struct expression *store_value;
	struct expression *objectref;
	struct compilation_unit *cu;
	struct fieldblock field;
	struct basic_block *bb;
	struct statement *stmt;
	struct insn *insn;

	field.offset = 8;
	objectref    = local_expr(J_REFERENCE, 0);
	store_target = instance_field_expr(J_INT, &field, objectref);
	store_value  = value_expr(J_INT, 0xcafebabe);
	stmt = alloc_statement(STMT_STORE);
	stmt->store_dest = &store_target->node;
	stmt->store_src  = &store_value->node;

	cu = alloc_compilation_unit(&method);
	bb = get_basic_block(cu, 0, 1);
	bb_add_stmt(bb, stmt);
	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EBP, -4, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_reg_reg_insn(INSN_MOV_REG_REG, REG_EAX, REG_ECX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_ADD_IMM_REG, sizeof(struct object), REG_ECX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_MOV_IMM_REG, field.offset, REG_EDX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_MOV_IMM_REG, 0xcafebabe, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_reg_memindex_insn(INSN_MOV_REG_MEMINDEX, REG_EAX, REG_ECX, REG_EDX, 2, insn);

	free_compilation_unit(cu);
}

static void assert_store_field_to_local(long expected_disp, unsigned long local_idx)
{
	struct expression *store_dest, *store_src;
	struct compilation_unit *cu;
	struct fieldblock field;
	struct statement *stmt;
	struct basic_block *bb;
	struct insn *insn;
	struct methodblock method = {
		.args_count = 0,
	};

	store_dest = local_expr(J_INT, local_idx);
	store_src  = class_field_expr(J_INT, &field);

	stmt = alloc_statement(STMT_STORE);
	stmt->store_dest = &store_dest->node;
	stmt->store_src  = &store_src->node;

	cu = alloc_compilation_unit(&method);
	bb = get_basic_block(cu, 0, 1);
	bb_add_stmt(bb, stmt);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_MOV_IMM_REG, (unsigned long) &field, REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_membase_reg_insn(INSN_MOV_MEMBASE_REG, REG_EAX, offsetof(struct fieldblock, static_value), REG_EAX, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_reg_membase_insn(INSN_MOV_REG_MEMBASE, REG_EAX, REG_EBP, expected_disp, insn);

	free_compilation_unit(cu);
}

void test_select_store_field_to_local(void)
{
	assert_store_field_to_local(-4, 0);
	assert_store_field_to_local(-8, 1);
}

void test_select_new(void)
{
	struct object *instance_class;
	struct compilation_unit *cu;
	struct expression *expr;
	struct statement *stmt;
	struct basic_block *bb;
	struct insn *insn;

	instance_class = new_class();
	expr = new_expr(instance_class);
	stmt = alloc_statement(STMT_EXPRESSION);
	stmt->expression = &expr->node;

	cu = alloc_compilation_unit(NULL);
	bb = get_basic_block(cu, 0, 1);
	bb_add_stmt(bb, stmt);

	insn_select(bb);

	insn = list_first_entry(&bb->insn_list, struct insn, insn_list_node);
	assert_imm_insn(INSN_PUSH_IMM, (unsigned long) instance_class, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_rel_insn(INSN_CALL_REL, (unsigned long) allocObject, insn);

	insn = list_next_entry(&insn->insn_list_node, struct insn, insn_list_node);
	assert_imm_reg_insn(INSN_ADD_IMM_REG, 4, REG_ESP, insn);

	free(instance_class);
	free_compilation_unit(cu);
}
