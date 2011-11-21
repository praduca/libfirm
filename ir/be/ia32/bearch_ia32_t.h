/*
 * Copyright (C) 1995-2008 University of Karlsruhe.  All right reserved.
 *
 * This file is part of libFirm.
 *
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * Licensees holding valid libFirm Professional Edition licenses may use
 * this file in accordance with the libFirm Commercial License.
 * Agreement provided with the Software.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.
 */

/**
 * @file
 * @brief       This is the main ia32 firm backend driver.
 * @author      Christian Wuerdig
 */
#ifndef FIRM_BE_IA32_BEARCH_IA32_T_H
#define FIRM_BE_IA32_BEARCH_IA32_T_H

#include "config.h"
#include "pmap.h"
#include "debug.h"
#include "ia32_nodes_attr.h"
#include "set.h"
#include "pdeq.h"

#include "be.h"
#include "beemitter.h"
#include "gen_ia32_regalloc_if.h"

#ifdef NDEBUG
#define SET_IA32_ORIG_NODE(n, o)
#else  /* ! NDEBUG */
#define SET_IA32_ORIG_NODE(n, o) set_ia32_orig_node(n, o)
#endif /* NDEBUG */

typedef struct ia32_isa_t            ia32_isa_t;
typedef struct ia32_irn_ops_t        ia32_irn_ops_t;
typedef struct ia32_intrinsic_env_t  ia32_intrinsic_env_t;

typedef struct ia32_irg_data_t {
	ir_node  **blk_sched;    /**< an array containing the scheduled blocks */
	unsigned do_x87_sim:1;   /**< set to 1 if x87 simulation should be enforced */
	unsigned dump:1;         /**< set to 1 if graphs should be dumped */
	ir_node  *noreg_gp;       /**< unique NoReg_GP node */
	ir_node  *noreg_vfp;      /**< unique NoReg_VFP node */
	ir_node  *noreg_xmm;      /**< unique NoReg_XMM node */

	ir_node  *fpu_trunc_mode; /**< truncate fpu mode */
	ir_node  *get_eip;        /**< get eip node */
} ia32_irg_data_t;

/**
 * IA32 ISA object
 */
struct ia32_isa_t {
	arch_env_t             base;     /**< must be derived from arch_env_t */
	pmap                  *tv_ent;   /**< A map of entities that store const tarvals */
	int                    fpu_arch; /**< FPU architecture */
};

/**
 * A helper type collecting needed info for IA32 intrinsic lowering.
 */
struct ia32_intrinsic_env_t {
	ia32_isa_t *isa;     /**< the isa object */
	ir_graph   *irg;     /**< the irg, these entities belong to */
	ir_entity  *divdi3;  /**< entity for __divdi3 library call */
	ir_entity  *moddi3;  /**< entity for __moddi3 library call */
	ir_entity  *udivdi3; /**< entity for __udivdi3 library call */
	ir_entity  *umoddi3; /**< entity for __umoddi3 library call */
};

typedef enum transformer_t {
	TRANSFORMER_DEFAULT,
#ifdef FIRM_GRGEN_BE
	TRANSFORMER_PBQP,
	TRANSFORMER_RAND
#endif
} transformer_t;

#ifdef FIRM_GRGEN_BE
/** The selected transformer. */
extern transformer_t be_transformer;

#else
#define be_transformer TRANSFORMER_DEFAULT
#endif

/** The mode for the floating point control word. */
extern ir_mode *ia32_mode_fpcw;
/** extended floatingpoint mode */
extern ir_mode *ia32_mode_E;
extern ir_type *ia32_type_E;

static inline ia32_irg_data_t *ia32_get_irg_data(const ir_graph *irg)
{
	return (ia32_irg_data_t*) be_birg_from_irg(irg)->isa_link;
}

/**
 * Returns the unique per irg GP NoReg node.
 */
ir_node *ia32_new_NoReg_gp(ir_graph *irg);
ir_node *ia32_new_NoReg_xmm(ir_graph *irg);
ir_node *ia32_new_NoReg_vfp(ir_graph *irg);

/**
 * Returns the unique per irg FPU truncation mode node.
 */
ir_node *ia32_new_Fpu_truncate(ir_graph *irg);

/**
 * Split instruction with source AM into Load and separate instruction.
 * @return result of the Load
 */
ir_node *ia32_turn_back_am(ir_node *node);

/**
 * Maps all intrinsic calls that the backend support
 * and map all instructions the backend did not support
 * to runtime calls.
 */
void ia32_handle_intrinsics(void);

/**
 * Ia32 implementation.
 *
 * @param method   the method type of the emulation function entity
 * @param op       the emulated ir_op
 * @param imode    the input mode of the emulated opcode
 * @param omode    the output mode of the emulated opcode
 * @param context  the context parameter
 */
ir_entity *ia32_create_intrinsic_fkt(ir_type *method, const ir_op *op,
                                     const ir_mode *imode, const ir_mode *omode,
                                     void *context);

/**
 * Return the stack entity that contains the return address.
 */
ir_entity *ia32_get_return_address_entity(ir_graph *irg);

/**
 * Return the stack entity that contains the frame address.
 */
ir_entity *ia32_get_frame_address_entity(ir_graph *irg);

#endif
