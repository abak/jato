/*
 * IA-32 code emitter.
 *
 * Copyright (C) 2006  Pekka Enberg
 *
 * This file is released under the GPL version 2. Please refer to the file
 * LICENSE for details.
 */

#include <jit/basic-block.h>
#include <jit/instruction.h>
#include <jit/statement.h>
#include <vm/buffer.h>
#include <x86-objcode.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 *	encode_reg:	Encode register to be used in IA-32 instruction.
 *	@reg: Register to encode.
 *
 *	Returns register in r/m or reg/opcode field format of the ModR/M byte.
 */
static unsigned char encode_reg(enum machine_reg reg)
{
	unsigned char ret = 0;

	switch (reg) {
	case REG_EAX:
		ret = 0x00;
		break;
	case REG_EBX:
		ret = 0x03;
		break;
	case REG_ECX:
		ret = 0x01;
		break;
	case REG_EDX:
		ret = 0x02;
		break;
	case REG_ESP:
		ret = 0x04;
		break;
	case REG_EBP:
		ret = 0x05;
		break;
	}
	return ret;
}

static inline bool is_imm_8(long imm)
{
	return (imm >= -128) && (imm <= 127);
}

/**
 *	encode_modrm:	Encode a ModR/M byte of an IA-32 instruction.
 *	@mod: The mod field of the byte.
 *	@reg_opcode: The reg/opcode field of the byte.
 *	@rm: The r/m field of the byte.
 */
static inline unsigned char encode_modrm(unsigned char mod,
					 unsigned char reg_opcode,
					 unsigned char rm)
{
	return ((mod & 0x3) << 6) | ((reg_opcode & 0x7) << 3) | (rm & 0x7);
}

static inline unsigned char encode_sib(unsigned char scale,
				       unsigned char index, unsigned char base)
{
	return ((scale & 0x3) << 6) | ((index & 0x7) << 3) | (base & 0x7);
}

static inline void emit(struct buffer *buf, unsigned char c)
{
	int err;

	err = append_buffer(buf, c);
	assert(!err);
}

static void write_imm32(struct buffer *buf, unsigned long offset, long imm32)
{
	unsigned char *buffer;
	union {
		int val;
		unsigned char b[4];
	} imm_buf;

	buffer = buf->buf;
	imm_buf.val = imm32;

	buffer[offset] = imm_buf.b[0];
	buffer[offset + 1] = imm_buf.b[1];
	buffer[offset + 2] = imm_buf.b[2];
	buffer[offset + 3] = imm_buf.b[3];
}

static void emit_imm32(struct buffer *buf, int imm)
{
	union {
		int val;
		unsigned char b[4];
	} imm_buf;

	imm_buf.val = imm;
	emit(buf, imm_buf.b[0]);
	emit(buf, imm_buf.b[1]);
	emit(buf, imm_buf.b[2]);
	emit(buf, imm_buf.b[3]);
}

static void emit_imm(struct buffer *buf, long imm)
{
	if (is_imm_8(imm))
		emit(buf, imm);
	else
		emit_imm32(buf, imm);
}

static void emit_membase_reg(struct buffer *buf, unsigned char opc,
			     union operand *src, union operand *dest)
{
	enum machine_reg base_reg, dest_reg;
	unsigned long disp;
	unsigned char mod, rm, mod_rm;
	int needs_sib;

	base_reg = src->base_reg->reg;
	disp = src->disp;
	dest_reg = dest->reg->reg;

	needs_sib = (base_reg == REG_ESP);

	emit(buf, opc);

	if (needs_sib)
		rm = 0x04;
	else
		rm = encode_reg(base_reg);

	if (is_imm_8(disp))
		mod = 0x01;
	else
		mod = 0x02;

	mod_rm = encode_modrm(mod, encode_reg(dest_reg), rm);
	emit(buf, mod_rm);

	if (needs_sib)
		emit(buf, encode_sib(0x00, 0x04, encode_reg(base_reg)));

	emit_imm(buf, disp);
}

static void __emit_push_reg(struct buffer *buf, enum machine_reg reg)
{
	emit(buf, 0x50 + encode_reg(reg));
}

static void emit_push_reg(struct buffer *buf, union operand *operand)
{
	__emit_push_reg(buf, operand->reg->reg);
}

static void __emit_mov_reg_reg(struct buffer *buf, enum machine_reg src_reg,
			       enum machine_reg dest_reg)
{
	unsigned char mod_rm;

	mod_rm = encode_modrm(0x03, encode_reg(src_reg), encode_reg(dest_reg));
	emit(buf, 0x89);
	emit(buf, mod_rm);
}

static void emit_mov_reg_reg(struct buffer *buf, union operand *src,
			     union operand *dest)
{
	__emit_mov_reg_reg(buf, src->reg->reg, dest->reg->reg);
}

static void emit_mov_membase_reg(struct buffer *buf,
				 union operand *src, union operand *dest)
{
	emit_membase_reg(buf, 0x8b, src, dest);
}

static void emit_mov_memindex_reg(struct buffer *buf,
				  union operand *src, union operand *dest)
{
	emit(buf, 0x8b);
	emit(buf, encode_modrm(0x00, encode_reg(dest->reg->reg), 0x04));
	emit(buf, encode_sib(src->shift, encode_reg(src->index_reg->reg), encode_reg(src->base_reg->reg)));
}

static void emit_mov_imm_reg(struct buffer *buf, union operand *src,
			     union operand *dest)
{
	emit(buf, 0xb8 + encode_reg(dest->reg->reg));
	emit_imm32(buf, src->imm);
}

static void emit_mov_imm_membase(struct buffer *buf, union operand *src,
				 union operand *dest)
{
	unsigned long mod = 0x00;

	emit(buf, 0xc7);

	if (dest->disp != 0)
		mod = 0x01;

	emit(buf, encode_modrm(mod, 0x00, encode_reg(dest->base_reg->reg)));

	if (dest->disp != 0)
		emit(buf, dest->disp);

	emit_imm32(buf, src->imm);
}

static void emit_mov_reg_membase(struct buffer *buf, union operand *src,
				 union operand *dest)
{
	int mod;

	if (is_imm_8(dest->disp))
		mod = 0x01;
	else
		mod = 0x02;

	emit(buf, 0x89);
	emit(buf, encode_modrm(mod, encode_reg(src->reg->reg),
			       encode_reg(dest->base_reg->reg)));

	emit_imm(buf, dest->disp);
}

static void emit_mov_reg_memindex(struct buffer *buf, union operand *src,
				  union operand *dest)
{
	emit(buf, 0x89);
	emit(buf, encode_modrm(0x00, encode_reg(src->reg->reg), 0x04));
	emit(buf, encode_sib(dest->shift, encode_reg(dest->index_reg->reg), encode_reg(dest->base_reg->reg)));
}

static void emit_alu_imm_reg(struct buffer *buf, unsigned char opc_ext,
			     long imm, enum machine_reg reg)
{
	int opc;

	if (is_imm_8(imm))
		opc = 0x83;
	else
		opc = 0x81;

	emit(buf, opc);
	emit(buf, encode_modrm(0x3, opc_ext, encode_reg(reg)));
	emit_imm(buf, imm);
}

static void emit_sub_imm_reg(struct buffer *buf, unsigned long imm,
			     enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 0x05, imm, reg);
}

void emit_prolog(struct buffer *buf, unsigned long nr_locals)
{
	__emit_push_reg(buf, REG_EBP);
	__emit_mov_reg_reg(buf, REG_ESP, REG_EBP);

	if (nr_locals)
		emit_sub_imm_reg(buf, nr_locals * sizeof(unsigned long), REG_ESP);
}

static void emit_pop_reg(struct buffer *buf, enum machine_reg reg)
{
	emit(buf, 0x58 + encode_reg(reg));
}

static void __emit_push_imm(struct buffer *buf, long imm)
{
	unsigned char opc;

	if (is_imm_8(imm))
		opc = 0x6a;
	else
		opc = 0x68;

	emit(buf, opc);
	emit_imm(buf, imm);
}

static void emit_push_imm(struct buffer *buf, union operand *operand)
{
	__emit_push_imm(buf, operand->imm);
}

#define CALL_INSN_SIZE 5

static void __emit_call(struct buffer *buf, void *call_target)
{
	int disp = call_target - buffer_current(buf) - CALL_INSN_SIZE;

	emit(buf, 0xe8);
	emit_imm32(buf, disp);
}

static void emit_call(struct buffer *buf, union operand *operand)
{
	__emit_call(buf, (void *)operand->rel);
}

void emit_ret(struct buffer *buf)
{
	emit(buf, 0xc3);
}

void emit_epilog(struct buffer *buf, unsigned long nr_locals)
{
	if (nr_locals)
		emit(buf, 0xc9);
	else
		emit_pop_reg(buf, REG_EBP);

	emit_ret(buf);
}

static void emit_add_membase_reg(struct buffer *buf,
				 union operand *src, union operand *dest)
{
	emit_membase_reg(buf, 0x03, src, dest);
}

static void emit_and_membase_reg(struct buffer *buf,
				 union operand *src, union operand *dest)
{
	emit_membase_reg(buf, 0x23, src, dest);
}

static void emit_sub_membase_reg(struct buffer *buf,
				 union operand *src, union operand *dest)
{
	emit_membase_reg(buf, 0x2b, src, dest);
}

static void __emit_div_mul_membase_reg(struct buffer *buf,
				       union operand *src,
				       union operand *dest,
				       unsigned char opc_ext)
{
	enum machine_reg reg;
	long disp;
	int mod;

	assert(dest->reg->reg == REG_EAX);

	reg = src->base_reg->reg;
	disp = src->disp;

	if (is_imm_8(disp))
		mod = 0x01;
	else
		mod = 0x02;

	emit(buf, 0xf7);
	emit(buf, encode_modrm(mod, opc_ext, encode_reg(reg)));
	emit_imm(buf, disp);
}

static void emit_mul_membase_reg(struct buffer *buf,
				 union operand *src, union operand *dest)
{
	__emit_div_mul_membase_reg(buf, src, dest, 0x04);
}

static void emit_neg_reg(struct buffer *buf, union operand *operand)
{
	emit(buf, 0xf7);
	emit(buf, encode_modrm(0x3, 0x3, encode_reg(operand->reg->reg)));
}

static void emit_cltd_reg_reg(struct buffer *buf, union operand *src, union operand *dest)
{
	assert(src->reg->reg == REG_EAX);
	assert(dest->reg->reg == REG_EDX);

	emit(buf, 0x99);
}

static void emit_div_membase_reg(struct buffer *buf, union operand *src,
				 union operand *dest)
{
	__emit_div_mul_membase_reg(buf, src, dest, 0x07);
}

static void __emit_shift_reg_reg(struct buffer *buf,
				 union operand *src,
				 union operand *dest, unsigned char opc_ext)
{
	assert(src->reg->reg == REG_ECX);

	emit(buf, 0xd3);
	emit(buf, encode_modrm(0x03, opc_ext, encode_reg(dest->reg->reg)));
}

static void emit_shl_reg_reg(struct buffer *buf, union operand *src,
			     union operand *dest)
{
	__emit_shift_reg_reg(buf, src, dest, 0x04);
}

static void emit_sar_reg_reg(struct buffer *buf, union operand *src,
			     union operand *dest)
{
	__emit_shift_reg_reg(buf, src, dest, 0x07);
}

static void emit_shr_reg_reg(struct buffer *buf, union operand *src,
			     union operand *dest)
{
	__emit_shift_reg_reg(buf, src, dest, 0x05);
}

static void emit_or_membase_reg(struct buffer *buf,
				union operand *src, union operand *dest)
{
	emit_membase_reg(buf, 0x0b, src, dest);
}

static void __emit_add_imm_reg(struct buffer *buf, long imm, enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 0x00, imm, reg);
}

static void emit_add_imm_reg(struct buffer *buf,
			     union operand *src, union operand *dest)
{
	__emit_add_imm_reg(buf, src->imm, dest->reg->reg);
}

static void emit_cmp_imm_reg(struct buffer *buf, union operand *src,
			     union operand *dest)
{
	emit_alu_imm_reg(buf, 0x07, src->imm, dest->reg->reg);
}

static void emit_cmp_membase_reg(struct buffer *buf, union operand *src, union operand *dest)
{
	emit_membase_reg(buf, 0x3b, src, dest);
}

static void emit_indirect_jump_reg(struct buffer *buf, enum machine_reg reg)
{
	emit(buf, 0xff);
	emit(buf, encode_modrm(0x3, 0x04, encode_reg(reg)));
}

void emit_branch_rel(struct buffer *buf, unsigned char prefix,
		     unsigned char opc, long rel32)
{
	if (prefix)
		emit(buf, prefix);
	emit(buf, opc);
	emit_imm32(buf, rel32);
}

#define PREFIX_SIZE 1
#define BRANCH_INSN_SIZE 5
#define BRANCH_TARGET_OFFSET 1

static long branch_rel_addr(struct insn *insn, unsigned long target_offset)
{
	long ret;

	ret = target_offset - insn->offset - BRANCH_INSN_SIZE;
	if (insn->escaped)
		ret -= PREFIX_SIZE;

	return ret;
}

static void __emit_branch(struct buffer *buf, unsigned char prefix,
			  unsigned char opc, struct insn *insn)
{
	struct basic_block *target_bb;
	long addr = 0;

	if (prefix)
		insn->escaped = true;

	target_bb = insn->operand.branch_target;

	if (target_bb->is_emitted) {
		struct insn *target_insn =
		    list_first_entry(&target_bb->insn_list, struct insn,
			       insn_list_node);

		addr = branch_rel_addr(insn, target_insn->offset);
	} else
		list_add(&insn->branch_list_node, &target_bb->backpatch_insns);

	emit_branch_rel(buf, prefix, opc, addr);
}

static void emit_je_branch(struct buffer *buf, struct insn *insn)
{
	__emit_branch(buf, 0x0f, 0x84, insn);
}

static void emit_jne_branch(struct buffer *buf, struct insn *insn)
{
	__emit_branch(buf, 0x0f, 0x85, insn);
}

static void emit_jmp_branch(struct buffer *buf, struct insn *insn)
{
	__emit_branch(buf, 0x00, 0xe9, insn);
}

static void emit_indirect_call(struct buffer *buf, union operand *operand)
{
	emit(buf, 0xff);
	emit(buf, encode_modrm(0x0, 0x2, encode_reg(operand->reg->reg)));
}

static void emit_xor_membase_reg(struct buffer *buf,
				 union operand *src, union operand *dest)
{
	emit_membase_reg(buf, 0x33, src, dest);
}

enum emitter_type {
	NO_OPERANDS = 1,
	SINGLE_OPERAND,
	TWO_OPERANDS,
	BRANCH,
};

struct emitter {
	void *emit_fn;
	enum emitter_type type;
};

#define DECL_EMITTER(_insn_type, _fn, _emitter_type) \
	[_insn_type] = { .emit_fn = _fn, .type = _emitter_type }

static struct emitter emitters[] = {
	DECL_EMITTER(INSN_ADD_IMM_REG, emit_add_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_ADD_MEMBASE_REG, emit_add_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_AND_MEMBASE_REG, emit_and_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_CALL_REG, emit_indirect_call, SINGLE_OPERAND),
	DECL_EMITTER(INSN_CALL_REL, emit_call, SINGLE_OPERAND),
	DECL_EMITTER(INSN_CLTD_REG_REG, emit_cltd_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_CMP_IMM_REG, emit_cmp_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_CMP_MEMBASE_REG, emit_cmp_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_DIV_MEMBASE_REG, emit_div_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_JE_BRANCH, emit_je_branch, BRANCH),
	DECL_EMITTER(INSN_JNE_BRANCH, emit_jne_branch, BRANCH),
	DECL_EMITTER(INSN_JMP_BRANCH, emit_jmp_branch, BRANCH),
	DECL_EMITTER(INSN_MOV_IMM_MEMBASE, emit_mov_imm_membase, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_IMM_REG, emit_mov_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_MEMBASE_REG, emit_mov_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_MEMINDEX_REG, emit_mov_memindex_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_REG_MEMBASE, emit_mov_reg_membase, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_REG_MEMINDEX, emit_mov_reg_memindex, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_REG_REG, emit_mov_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MUL_MEMBASE_REG, emit_mul_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_NEG_REG, emit_neg_reg, SINGLE_OPERAND),
	DECL_EMITTER(INSN_OR_MEMBASE_REG, emit_or_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_PUSH_IMM, emit_push_imm, SINGLE_OPERAND),
	DECL_EMITTER(INSN_PUSH_REG, emit_push_reg, SINGLE_OPERAND),
	DECL_EMITTER(INSN_SAR_REG_REG, emit_sar_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SHL_REG_REG, emit_shl_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SHR_REG_REG, emit_shr_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SUB_MEMBASE_REG, emit_sub_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_XOR_MEMBASE_REG, emit_xor_membase_reg, TWO_OPERANDS),
};

typedef void (*emit_no_operands_fn) (struct buffer *);

static void emit_no_operands(struct emitter *emitter, struct buffer *buf)
{
	emit_no_operands_fn emit = emitter->emit_fn;
	emit(buf);
}

typedef void (*emit_single_operand_fn) (struct buffer *, union operand * operand);

static void emit_single_operand(struct emitter *emitter, struct buffer *buf, struct insn *insn)
{
	emit_single_operand_fn emit = emitter->emit_fn;
	emit(buf, &insn->operand);
}

typedef void (*emit_two_operands_fn) (struct buffer *, union operand * src, union operand * dest);

static void emit_two_operands(struct emitter *emitter, struct buffer *buf, struct insn *insn)
{
	emit_two_operands_fn emit = emitter->emit_fn;
	emit(buf, &insn->src, &insn->dest);
}

typedef void (*emit_branch_fn) (struct buffer *, struct insn *);

static void emit_branch(struct emitter *emitter, struct buffer *buf, struct insn *insn)
{
	emit_branch_fn emit = emitter->emit_fn;
	emit(buf, insn);
}

static void __emit_insn(struct buffer *buf, struct insn *insn)
{
	struct emitter *emitter;

	emitter = &emitters[insn->type];
	switch (emitter->type) {
	case NO_OPERANDS:
		emit_no_operands(emitter, buf);
		break;
	case SINGLE_OPERAND:
		emit_single_operand(emitter, buf, insn);
		break;
	case TWO_OPERANDS:
		emit_two_operands(emitter, buf, insn);
		break;
	case BRANCH:
		emit_branch(emitter, buf, insn);
		break;
	default:
		printf("Oops. No emitter for 0x%x.\n", insn->type);
		abort();
	};
}

static void emit_insn(struct buffer *buf, struct insn *insn)
{
	insn->offset = buffer_offset(buf);
	__emit_insn(buf, insn);
}

static void backpatch_branch_target(struct buffer *buf,
				    struct insn *insn,
				    unsigned long target_offset)
{
	unsigned long backpatch_offset;
	long relative_addr;

	backpatch_offset = insn->offset + BRANCH_TARGET_OFFSET;
	if (insn->escaped)
		backpatch_offset += PREFIX_SIZE;

	relative_addr = branch_rel_addr(insn, target_offset);

	write_imm32(buf, backpatch_offset, relative_addr);
}

static void backpatch_branches(struct buffer *buf,
			       struct list_head *to_backpatch,
			       unsigned long target_offset)
{
	struct insn *this, *next;

	list_for_each_entry_safe(this, next, to_backpatch, branch_list_node) {
		backpatch_branch_target(buf, this, target_offset);
		list_del(&this->branch_list_node);
	}
}

void emit_body(struct basic_block *bb, struct buffer *buf)
{
	struct insn *insn;

	backpatch_branches(buf, &bb->backpatch_insns, buffer_offset(buf));

	for_each_insn(insn, &bb->insn_list) {
		emit_insn(buf, insn);
	}
	bb->is_emitted = true;
}

void emit_trampoline(struct compilation_unit *cu, void *call_target,
		     struct buffer *buf)
{
	__emit_push_imm(buf, (unsigned long)cu);
	__emit_call(buf, call_target);
	__emit_add_imm_reg(buf, 0x04, REG_ESP);
	emit_indirect_jump_reg(buf, REG_EAX);
}
