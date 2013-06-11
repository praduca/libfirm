/*
 * This file is part of libFirm.
 * Copyright (C) 2012 University of Karlsruhe.
 */

/**
 * @file
 * @brief   Lowering of high level constructs.
 * @author  Michael Beck
 */
#ifndef FIRM_LOWERING_H
#define FIRM_LOWERING_H

#include <stddef.h>

#include "firm_types.h"

#include "begin.h"

/**
 * @defgroup ir_lowering  Lowering
 *
 * Lowering is the process of transforming a highlevel representation
 * (a representation closer to the sourcecode) into a lower-level representation
 * (something closer to the target machine).
 *
 * @{
 */

/**
 * Lower small CopyB nodes to Load/Store nodes, preserve medium-sized CopyB
 * nodes and replace large CopyBs by a call to memcpy, depending on the given
 * parameters.
 *
 * Small CopyB nodes (size <= max_small_size) are turned into a series of
 * loads and stores.
 * Medium-sized CopyB nodes (max_small_size < size < min_large_size) are
 * left untouched.
 * Large CopyB nodes (size >= min_large_size) are turned into a memcpy call.
 *
 * @param irg                 The graph to be lowered.
 * @param max_small_size      The maximum number of bytes for a CopyB node so
 *                            that it is still considered 'small'.
 * @param min_large_size      The minimum number of bytes for a CopyB node so
 *                            that it is regarded as 'large'.
 * @param allow_misalignments Backend can handle misaligned loads and stores.
 */
FIRM_API void lower_CopyB(ir_graph *irg, unsigned max_small_size,
                          unsigned min_large_size, int allow_misalignments);

/**
 * Lowers all Switches (Cond nodes with non-boolean mode) depending on spare_size.
 * They will either remain the same or be converted into if-cascades.
 *
 * @param irg        The ir graph to be lowered.
 * @param small_switch  If switch has <= cases then change it to an if-cascade.
 * @param spare_size Allowed spare size for table switches in machine words.
 *                   (Default in edgfe: 128)
 * @param selector_mode mode which must be used for Switch selector
 */
FIRM_API void lower_switch(ir_graph *irg, unsigned small_switch,
                           unsigned spare_size, ir_mode *selector_mode);

/**
 * Replaces SymConsts by a real constant if possible.
 * Replaces Sel nodes by address computation.  Also resolves array access.
 * Handle bit fields by added And/Or calculations.
 *
 * @param irg               the graph to lower
 *
 * @note: There is NO lowering ob objects oriented types. This is highly compiler
 *        and ABI specific and should be placed directly in the compiler.
 */
FIRM_API void lower_highlevel_graph(ir_graph *irg);

/**
 * Replaces SymConsts by a real constant if possible.
 * Replaces Sel nodes by address computation.  Also resolves array access.
 * Handle bit fields by added And/Or calculations.
 * Lowers all graphs.
 *
 * @note There is NO lowering of objects oriented types. This is highly compiler
 *       and ABI specific and should be placed directly in the compiler.
 */
FIRM_API void lower_highlevel(void);

/**
 * does the same as lower_highlevel for all nodes on the const code irg
 */
FIRM_API void lower_const_code(void);

/**
 * Used as callback, whenever a lowerable mux is found. The return value
 * indicates, whether the mux should be lowered. This may be used, to lower
 * floating point muxes, while keeping mux nodes for integers, for example.
 *
 * @param mux  The mux node that may be lowered.
 * @return     A non-zero value indicates that the mux should be lowered.
 */
typedef int lower_mux_callback(ir_node* mux);

/**
 * Lowers all mux nodes in the given graph. A callback function may be
 * given, to select the mux nodes to lower.
 *
 * @param irg      The graph to lower mux nodes in.
 * @param cb_func  The callback function for mux selection. Can be NULL,
 *                 to lower all mux nodes.
 */
FIRM_API void lower_mux(ir_graph *irg, lower_mux_callback *cb_func);

/**
 * An intrinsic mapper function.
 *
 * @param node   the IR-node that will be mapped
 * @return  non-zero if the call was mapped
 */
typedef int (i_mapper_func)(ir_node *node);

/** kind of an instruction record
 * @see #i_record */
enum ikind {
	INTRINSIC_CALL  = 0,  /**< the record represents an intrinsic call */
	INTRINSIC_INSTR       /**< the record represents an intrinsic instruction */
};

/**
 * An intrinsic call record.
 */
typedef struct i_call_record {
	enum ikind    kind;       /**< must be INTRINSIC_CALL */
	ir_entity     *i_ent;     /**< the entity representing an intrinsic call */
	i_mapper_func *i_mapper;  /**< the mapper function to call */
} i_call_record;

/**
 * An intrinsic instruction record.
 */
typedef struct i_instr_record {
	enum ikind    kind;       /**< must be INTRINSIC_INSTR */
	ir_op         *op;        /**< the opcode that must be mapped. */
	i_mapper_func *i_mapper;  /**< the mapper function to call */
} i_instr_record;

/**
 * An intrinsic record.
 */
typedef union i_record {
	enum ikind     kind;     /**< kind of record */
	i_call_record  i_call;   /**< used for call records */
	i_instr_record i_instr;  /**< used for isnstruction records */
} i_record;

typedef struct ir_intrinsics_map ir_intrinsics_map;

/**
 * Create a new intrinsic lowering map which can be used by ir_lower_intrinsics.
 *
 * @param list             an array of intrinsic map records
 * @param length           the length of the array
 * @param part_block_used  set to 1 if part_block() is used by one of the
 *                         lowering functions.
 */
FIRM_API ir_intrinsics_map *ir_create_intrinsics_map(i_record *list,
                                                     size_t length,
                                                     int part_block_used);

/**
 * Frees resources occupied by an intrisics_map created by
 * ir_create_intrinsics_map().
 */
FIRM_API void ir_free_intrinsics_map(ir_intrinsics_map *map);

/**
 * Go through all graphs and map calls to intrinsic functions and instructions.
 *
 * Every call or instruction is reported to its mapper function,
 * which is responsible for rebuilding the graph.
 */
FIRM_API void ir_lower_intrinsics(ir_graph *irg, ir_intrinsics_map *map);

/**
 * A mapper for the integer/float absolute value: type abs(type v).
 * Replaces the call by a Abs node.
 *
 * @return always 1
 */
FIRM_API int i_mapper_abs(ir_node *call);

/**
 * A mapper for the integer byte swap value: type bswap(type v).
 * Replaces the call by a builtin[ir_bk_bswap] node.
 *
 * @return always 1
 */
FIRM_API int i_mapper_bswap(ir_node *call);

/**
 * A mapper for the floating point sqrt(v): floattype sqrt(floattype v);
 *
 * @return 1 if the sqrt call was removed, 0 else.
 */
FIRM_API int i_mapper_sqrt(ir_node *call);

/**
 * A mapper for the floating point cbrt(v): floattype sqrt(floattype v);
 *
 * @return 1 if the cbrt call was removed, 0 else.
 */
FIRM_API int i_mapper_cbrt(ir_node *call);

/**
 * A mapper for the floating point pow(a, b): floattype pow(floattype a, floattype b);
 *
 * @return 1 if the pow call was removed, 0 else.
 */
FIRM_API int i_mapper_pow(ir_node *call);

/**
 * A mapper for the floating point exp(a): floattype exp(floattype a);
 *
 * @return 1 if the exp call was removed, 0 else.
 */
FIRM_API int i_mapper_exp(ir_node *call);

/**
 * A mapper for the floating point exp2(a): floattype exp2(floattype a);
 *
 * @return 1 if the exp call was removed, 0 else.
 */
FIRM_API int i_mapper_exp2(ir_node *call);

/**
 * A mapper for the floating point exp10(a): floattype exp10(floattype a);
 *
 * @return 1 if the exp call was removed, 0 else.
 */
FIRM_API int i_mapper_exp10(ir_node *call);

/**
 * A mapper for the floating point log(a): floattype log(floattype a);
 *
 * @return 1 if the log call was removed, 0 else.
 */
FIRM_API int i_mapper_log(ir_node *call);

/**
 * A mapper for the floating point log(a): floattype log(floattype a);
 *
 * @return 1 if the log call was removed, 0 else.
 */
FIRM_API int i_mapper_log2(ir_node *call);

/**
 * A mapper for the floating point log(a): floattype log(floattype a);
 *
 * @return 1 if the log call was removed, 0 else.
 */
FIRM_API int i_mapper_log10(ir_node *call);

/**
 * A mapper for the floating point sin(a): floattype sin(floattype a);
 *
 * @return 1 if the sin call was removed, 0 else.
 */
FIRM_API int i_mapper_sin(ir_node *call);

/**
 * A mapper for the floating point sin(a): floattype cos(floattype a);
 *
 * @return 1 if the cos call was removed, 0 else.
 */
FIRM_API int i_mapper_cos(ir_node *call);

/**
 * A mapper for the floating point tan(a): floattype tan(floattype a);
 *
 * @return 1 if the tan call was removed, 0 else.
 */
FIRM_API int i_mapper_tan(ir_node *call);

/**
 * A mapper for the floating point asin(a): floattype asin(floattype a);
 *
 * @return 1 if the asin call was removed, 0 else.
 */
FIRM_API int i_mapper_asin(ir_node *call);

/**
 * A mapper for the floating point acos(a): floattype acos(floattype a);
 *
 * @return 1 if the tan call was removed, 0 else.
 */
FIRM_API int i_mapper_acos(ir_node *call);

/**
 * A mapper for the floating point atan(a): floattype atan(floattype a);
 *
 * @return 1 if the atan call was removed, 0 else.
 */
FIRM_API int i_mapper_atan(ir_node *call);

/**
 * A mapper for the floating point sinh(a): floattype sinh(floattype a);
 *
 * @return 1 if the sinh call was removed, 0 else.
 */
FIRM_API int i_mapper_sinh(ir_node *call);

/**
 * A mapper for the floating point cosh(a): floattype cosh(floattype a);
 *
 * @return 1 if the cosh call was removed, 0 else.
 */
FIRM_API int i_mapper_cosh(ir_node *call);

/**
 * A mapper for the floating point tanh(a): floattype tanh(floattype a);
 *
 * @return 1 if the tanh call was removed, 0 else.
 */
FIRM_API int i_mapper_tanh(ir_node *call);

/**
 * A mapper for the strcmp-Function: inttype strcmp(char pointer a, char pointer b);
 *
 * @return 1 if the strcmp call was removed, 0 else.
 */
FIRM_API int i_mapper_strcmp(ir_node *call);

/**
 * A mapper for the strncmp-Function: inttype strncmp(char pointer a, char pointer b, inttype len);
 *
 * @return 1 if the strncmp call was removed, 0 else.
 */
FIRM_API int i_mapper_strncmp(ir_node *call);

/**
 * A mapper for the strcpy-Function: char pointer strcpy(char pointer a, char pointer b);
 *
 * @return 1 if the strcpy call was removed, 0 else.
 */
FIRM_API int i_mapper_strcpy(ir_node *call);

/**
 * A mapper for the strlen-Function: inttype strlen(char pointer a);
 *
 * @return 1 if the strlen call was removed, 0 else.
 */
FIRM_API int i_mapper_strlen(ir_node *call);

/**
 * A mapper for the memcpy-Function: void pointer memcpy(void pointer d, void pointer s, inttype c);
 *
 * @return 1 if the memcpy call was removed, 0 else.
 */
FIRM_API int i_mapper_memcpy(ir_node *call);

/**
 * A mapper for the mempcpy-Function: void pointer mempcpy(void pointer d, void pointer s, inttype c);
 *
 * @return 1 if the mempcpy call was removed, 0 else.
 */
FIRM_API int i_mapper_mempcpy(ir_node *call);

/**
 * A mapper for the memmove-Function: void pointer memmove(void pointer d, void pointer s, inttype c);
 *
 * @return 1 if the memmove call was removed, 0 else.
 */
FIRM_API int i_mapper_memmove(ir_node *call);

/**
 * A mapper for the memset-Function: void pointer memset(void pointer d, inttype C, inttype len);
 *
 * @return 1 if the memset call was removed, 0 else.
 */
FIRM_API int i_mapper_memset(ir_node *call);

/**
 * A mapper for the strncmp-Function: inttype memcmp(void pointer a, void pointer b, inttype len);
 *
 * @return 1 if the strncmp call was removed, 0 else.
 */
FIRM_API int i_mapper_memcmp(ir_node *call);

/**
 * A mapper for the alloca() function: pointer alloca(inttype size)
 * Replaces the call by a Alloca(stack_alloc) node.
 *
 * @return always 1
 */
FIRM_API int i_mapper_alloca(ir_node *call);

/** @} */

#include "end.h"

#endif
