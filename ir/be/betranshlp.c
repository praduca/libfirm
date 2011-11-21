/*
 * Copyright (C) 1995-2010 University of Karlsruhe.  All right reserved.
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
 * @brief       be transform helper extracted from the ia32 backend.
 * @author      Matthias Braun, Michael Beck
 * @date        14.06.2007
 */
#include "config.h"

#include "pdeq.h"
#include "irop_t.h"
#include "iropt_t.h"
#include "irnode_t.h"
#include "irgraph_t.h"
#include "ircons_t.h"
#include "irhooks.h"
#include "iredges.h"
#include "irouts.h"
#include "trouts.h"
#include "cgana.h"
#include "debug.h"

#include "beirg.h"
#include "beabi.h"
#include "betranshlp.h"
#include "belive.h"
#include "benode.h"

typedef struct be_transform_env_t {
	ir_graph *irg;         /**< The irg, the node should be created in */
	waitq    *worklist;    /**< worklist of nodes that still need to be
	                            transformed */
	ir_node  *old_anchor;  /**< the old anchor node in the old irg */
} be_transform_env_t;


static be_transform_env_t env;

void be_set_transformed_node(ir_node *old_node, ir_node *new_node)
{
	ir_graph *irg = get_irn_irg(old_node);

	set_irn_link(old_node, new_node);
	mark_irn_visited(old_node);
	hook_dead_node_elim_subst(irg, old_node, new_node);
}

int be_is_transformed(const ir_node *node)
{
	return irn_visited(node);
}

static inline ir_node *be_get_transformed_node(ir_node *old_node)
{
	if (irn_visited(old_node)) {
		ir_node *new_node = (ir_node*)get_irn_link(old_node);
		assert(new_node != NULL);
		return new_node;
	}
	return NULL;
}

void be_duplicate_deps(ir_node *old_node, ir_node *new_node)
{
	int i;
	int deps = get_irn_deps(old_node);

	for (i = 0; i < deps; ++i) {
		ir_node *dep     = get_irn_dep(old_node, i);
		ir_node *new_dep = be_transform_node(dep);

		add_irn_dep(new_node, new_dep);
	}
}

void be_set_transform_function(ir_op *op, be_transform_func func)
{
	/* shouldn't be assigned twice (except for exchanging the default
	 * be_duplicate_node entries) */
	assert(op->ops.generic == NULL
			|| op->ops.generic == (op_func) be_duplicate_node);
	op->ops.generic = (op_func) func;
}

/**
 * Transform helper for blocks.
 */
static ir_node *transform_block(ir_node *node)
{
	ir_graph *irg  = get_irn_irg(node);
	dbg_info *dbgi = get_irn_dbg_info(node);
	ir_node  *block;

	block = new_ir_node(dbgi, irg, NULL, get_irn_op(node), get_irn_mode(node),
	                    get_irn_arity(node), get_irn_in(node) + 1);
	copy_node_attr(irg, node, block);
	block->node_nr = node->node_nr;

	/* put the preds in the worklist */
	be_enqueue_preds(node);

	return block;
}

static ir_node *transform_end(ir_node *node)
{
	/* end has to be duplicated manually because we need a dynamic in array */
	ir_graph *irg   = get_irn_irg(node);
	dbg_info *dbgi  = get_irn_dbg_info(node);
	ir_node  *block = be_transform_node(get_nodes_block(node));
	int      i, arity;
	ir_node  *new_end;

	new_end = new_ir_node(dbgi, irg, block, op_End, mode_X, -1, NULL);
	copy_node_attr(irg, node, new_end);
	be_duplicate_deps(node, new_end);

	set_irg_end(irg, new_end);

	/* do not transform predecessors yet to keep the pre-transform
	 * phase from visiting all the graph */
	arity = get_irn_arity(node);
	for (i = 0; i < arity; ++i) {
		ir_node *in = get_irn_n(node, i);
		add_End_keepalive(new_end, in);
	}
	be_enqueue_preds(node);

	return new_end;
}

void be_start_transform_setup(void)
{
	clear_irp_opcodes_generic_func();

	be_set_transform_function(op_Bad,         be_duplicate_node);
	be_set_transform_function(op_be_Copy,     be_duplicate_node);
	be_set_transform_function(op_be_CopyKeep, be_duplicate_node);
	be_set_transform_function(op_be_IncSP,    be_duplicate_node);
	be_set_transform_function(op_be_Keep,     be_duplicate_node);
	be_set_transform_function(op_be_Return,   be_duplicate_node);
	be_set_transform_function(op_be_Start,    be_duplicate_node);
	be_set_transform_function(op_Block,       transform_block);
	be_set_transform_function(op_End,         transform_end);
	be_set_transform_function(op_NoMem,       be_duplicate_node);
	be_set_transform_function(op_Pin,         be_duplicate_node);
	be_set_transform_function(op_Start,       be_duplicate_node);
	be_set_transform_function(op_Sync,        be_duplicate_node);
}

ir_node *be_duplicate_node(ir_node *node)
{
	ir_node  *block = be_transform_node(get_nodes_block(node));
	ir_graph *irg   = env.irg;
	dbg_info *dbgi  = get_irn_dbg_info(node);
	ir_mode  *mode  = get_irn_mode(node);
	ir_op    *op    = get_irn_op(node);
	ir_node  *new_node;
	int      i, arity;

	arity = get_irn_arity(node);
	if (op->opar == oparity_dynamic) {
		new_node = new_ir_node(dbgi, irg, block, op, mode, -1, NULL);
		for (i = 0; i < arity; ++i) {
			ir_node *in = get_irn_n(node, i);
			in = be_transform_node(in);
			add_irn_n(new_node, in);
		}
	} else {
		ir_node **ins = ALLOCAN(ir_node*, arity);
		for (i = 0; i < arity; ++i) {
			ir_node *in = get_irn_n(node, i);
			ins[i] = be_transform_node(in);
		}

		new_node = new_ir_node(dbgi, irg, block, op, mode, arity, ins);
	}

	copy_node_attr(irg, node, new_node);
	be_duplicate_deps(node, new_node);

	new_node->node_nr = node->node_nr;
	return new_node;
}

ir_node *be_transform_node(ir_node *node)
{
	ir_op             *op;
	ir_node           *new_node = be_get_transformed_node(node);
	be_transform_func *transform;

	if (new_node != NULL)
		return new_node;

	DEBUG_ONLY(be_set_transformed_node(node, NULL);)

	op = get_irn_op(node);
	if (op->ops.generic == NULL) {
		panic("No transform function registered for node %+F.", node);
	}
	transform = (be_transform_func *)op->ops.generic;

	new_node = transform(node);
	assert(new_node != NULL);

	be_set_transformed_node(node, new_node);
	return new_node;
}

void be_enqueue_preds(ir_node *node)
{
	int i, arity;

	/* put the preds in the worklist */
	arity = get_irn_arity(node);
	for (i = 0; i < arity; ++i) {
		ir_node *pred = get_irn_n(node, i);
		pdeq_putr(env.worklist, pred);
	}
}

/**
 * Rewire nodes which are potential loops (like Phis) to avoid endless loops.
 */
static void fix_loops(ir_node *node)
{
	int i, arity;
	int changed;

	assert(node_is_in_irgs_storage(env.irg, node));

	if (irn_visited_else_mark(node))
		return;

	changed = 0;
	if (! is_Block(node)) {
		ir_node *block     = get_nodes_block(node);
		ir_node *new_block = (ir_node*)get_irn_link(block);

		if (new_block != NULL) {
			set_nodes_block(node, new_block);
			block = new_block;
			changed = 1;
		}

		fix_loops(block);
	}

	arity = get_irn_arity(node);
	for (i = 0; i < arity; ++i) {
		ir_node *in = get_irn_n(node, i);
		ir_node *nw = (ir_node*)get_irn_link(in);

		if (nw != NULL && nw != in) {
			set_irn_n(node, i, nw);
			in = nw;
			changed = 1;
		}

		fix_loops(in);
	}
	/* fix proj block */
	if (is_Proj(node)) {
		set_nodes_block(node, get_nodes_block(get_Proj_pred(node)));
		changed = 1;
	}

	arity = get_irn_deps(node);
	for (i = 0; i < arity; ++i) {
		ir_node *in = get_irn_dep(node, i);
		ir_node *nw = (ir_node*)get_irn_link(in);

		if (nw != NULL && nw != in) {
			set_irn_dep(node, i, nw);
			in = nw;
			changed = 1;
		}

		fix_loops(in);
	}

	if (changed) {
		identify_remember(node);
	}
}

ir_node *be_pre_transform_node(ir_node *place)
{
	if (place == NULL)
		return NULL;

	return be_transform_node(place);
}

static void pre_transform_anchor(ir_graph *irg, int anchor)
{
	ir_node *old_anchor_node = get_irn_n(env.old_anchor, anchor);
	ir_node *transformed     = be_transform_node(old_anchor_node);
	set_irg_anchor(irg, anchor, transformed);
}

/**
 * Transforms all nodes. Deletes the old obstack and creates a new one.
 */
static void transform_nodes(ir_graph *irg, arch_pretrans_nodes *pre_transform)
{
	int       i;
	ir_node  *old_end, *new_anchor;

	hook_dead_node_elim(irg, 1);

	inc_irg_visited(irg);

	env.irg        = irg;
	env.worklist   = new_waitq();
	env.old_anchor = irg->anchor;

	old_end = get_irg_end(irg);

	/* put all anchor nodes in the worklist */
	for (i = get_irg_n_anchors(irg) - 1; i >= 0; --i) {
		ir_node *anchor = get_irg_anchor(irg, i);

		if (anchor == NULL)
			continue;
		waitq_put(env.worklist, anchor);
	}

	new_anchor  = new_r_Anchor(irg);
	irg->anchor = new_anchor;

	/* pre transform some anchors (so they are available in the other transform
	 * functions) */
	pre_transform_anchor(irg, anchor_no_mem);
	pre_transform_anchor(irg, anchor_end_block);
	pre_transform_anchor(irg, anchor_end);
	pre_transform_anchor(irg, anchor_start_block);
	pre_transform_anchor(irg, anchor_start);
	pre_transform_anchor(irg, anchor_frame);

	if (pre_transform)
		pre_transform();

	/* process worklist (this should transform all nodes in the graph) */
	while (! waitq_empty(env.worklist)) {
		ir_node *node = (ir_node*)waitq_get(env.worklist);
		be_transform_node(node);
	}

	/* fix loops and set new anchors*/
	inc_irg_visited(irg);
	for (i = get_irg_n_anchors(irg) - 1; i >= 0; --i) {
		ir_node *anchor = get_irn_n(env.old_anchor, i);

		if (anchor == NULL)
			continue;

		anchor = (ir_node*)get_irn_link(anchor);
		fix_loops(anchor);
		set_irn_n(new_anchor, i, anchor);
	}

	del_waitq(env.worklist);
	free_End(old_end);
	hook_dead_node_elim(irg, 0);
}

void be_transform_graph(ir_graph *irg, arch_pretrans_nodes *func)
{
	ir_graph *old_current_ir_graph = current_ir_graph;
	struct obstack *old_obst = NULL;
	struct obstack *new_obst = NULL;

	current_ir_graph = irg;

	/* create a new obstack */
	old_obst = irg->obst;
	new_obst = XMALLOC(struct obstack);
	obstack_init(new_obst);
	irg->obst = new_obst;
	irg->last_node_idx = 0;

	free_vrp_data(irg);

	/* create new value table for CSE */
	new_identities(irg);

	/* do the main transformation */
	transform_nodes(irg, func);

	/* free the old obstack */
	obstack_free(old_obst, 0);
	xfree(old_obst);

	/* restore state */
	current_ir_graph = old_current_ir_graph;

	/* most analysis info is wrong after transformation */
	free_callee_info(irg);
	free_irg_outs(irg);
	free_trouts();
	free_loop_information(irg);
	clear_irg_state(irg, IR_GRAPH_STATE_CONSISTENT_DOMINANCE);

	be_liveness_invalidate(be_get_irg_liveness(irg));
	/* Hack for now, something is buggy with invalidate liveness... */
	be_birg_from_irg(irg)->lv = NULL;
	be_invalidate_dom_front(irg);

	/* recalculate edges */
	edges_deactivate(irg);
	edges_activate(irg);
}
