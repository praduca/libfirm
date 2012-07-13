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
 * @brief       Internal header for copy optimization problem.
 * @author      Daniel Grund
 * @date        12.04.2005
 */
#ifndef FIRM_BE_BECOPYOPT_T_H
#define FIRM_BE_BECOPYOPT_T_H

#include "obst.h"
#include "list.h"
#include "set.h"
#include "irnode_t.h"

#include "bearch.h"
#include "bechordal_t.h"
#include "becopyopt.h"

/**
 * Data representing the problem of copy minimization.
 */
struct copy_opt_t {
	be_chordal_env_t            *cenv;
	const arch_register_class_t *cls;
	ir_graph                    *irg;
	char                        *name;       /**< ProgName__IrgName__RegClassName */
	cost_fct_t                  get_costs;   /**< function ptr used to get costs for copies */

	/** Representation as optimization units */
	struct list_head units;  /**< all units to optimize in specific order */

	/** Representation in graph structure. Only build on demand */
	struct obstack obst;
	set    *nodes;
};

/* Helpers */
#define ASSERT_OU_AVAIL(co)     assert((co)->units.next && "Representation as optimization-units not build")
#define ASSERT_GS_AVAIL(co)     assert((co)->nodes && "Representation as graph not build")

static inline unsigned get_irn_col(const ir_node *node)
{
	return arch_register_get_index(arch_get_irn_register(node));
}

static inline void set_irn_col(const arch_register_class_t *cls, ir_node *node,
                               unsigned color)
{
	const arch_register_t *reg = arch_register_for_index(cls, color);
	arch_set_irn_register(node, reg);
}

#define list_entry_units(lh) list_entry(lh, unit_t, units)

#define is_Reg_Phi(irn)        (is_Phi(irn) && mode_is_data(get_irn_mode(irn)))

#define get_Perm_src(irn) (get_irn_n(get_Proj_pred(irn), get_Proj_proj(irn)))
#define is_Perm_Proj(irn) (is_Proj(irn) && be_is_Perm(get_Proj_pred(irn)))

static inline int is_2addr_code(const arch_register_req_t *req)
{
	return (req->type & arch_register_req_type_should_be_same) != 0;
}

/******************************************************************************
   ____        _   _    _       _ _          _____ _
  / __ \      | | | |  | |     (_) |        / ____| |
 | |  | |_ __ | |_| |  | |_ __  _| |_ ___  | (___ | |_ ___  _ __ __ _  __ _  ___
 | |  | | '_ \| __| |  | | '_ \| | __/ __|  \___ \| __/ _ \| '__/ _` |/ _` |/ _ \
 | |__| | |_) | |_| |__| | | | | | |_\__ \  ____) | || (_) | | | (_| | (_| |  __/
  \____/| .__/ \__|\____/|_| |_|_|\__|___/ |_____/ \__\___/|_|  \__,_|\__, |\___|
        | |                                                            __/ |
        |_|                                                           |___/
 ******************************************************************************/

#define MIS_HEUR_TRIGGER 8

typedef struct unit_t {
	struct list_head units;              /**< chain for all units */
	copy_opt_t       *co;                /**< the copy opt this unit belongs to */
	int              node_count;         /**< size of the nodes array */
	ir_node          **nodes;            /**< [0] is the root-node, others are non interfering args of it. */
	int              *costs;             /**< costs[i] are incurred, if nodes[i] has a different color */
	int              inevitable_costs;   /**< sum of costs of all args interfering with root */
	int              all_nodes_costs;    /**< sum of all costs[i] */
	int              min_nodes_costs;    /**< a lower bound for the costs in costs[], determined by a max independent set */
	int              sort_key;           /**< maximum costs. controls the order of ou's in the struct list_head units. */

	/* for heuristic */
	struct list_head queue;              /**< list of qn's sorted by weight of qn-mis */
} unit_t;



/******************************************************************************
   _____                 _        _____ _
  / ____|               | |      / ____| |
 | |  __ _ __ __ _ _ __ | |__   | (___ | |_ ___  _ __ __ _  __ _  ___
 | | |_ | '__/ _` | '_ \| '_ \   \___ \| __/ _ \| '__/ _` |/ _` |/ _ \
 | |__| | | | (_| | |_) | | | |  ____) | || (_) | | | (_| | (_| |  __/
  \_____|_|  \__,_| .__/|_| |_| |_____/ \__\___/|_|  \__,_|\__, |\___|
                  | |                                       __/ |
                  |_|                                      |___/
 ******************************************************************************/

typedef struct neighb_t neighb_t;
typedef struct affinity_node_t affinity_node_t;

struct neighb_t {
	neighb_t *next;   /** the next neighbour entry*/
	const ir_node  *irn;    /** the neighbour itself */
	int      costs;   /** the costs of the edge (affinity_node_t->irn, neighb_t->irn) */
};

struct affinity_node_t {
	const ir_node  *irn;          /** a node with affinity edges */
	int      degree;        /** number of affinity edges in the linked list below */
	neighb_t *neighbours;   /** a linked list of all affinity neighbours */
	void     *data;         /** stuff that is attachable. */
};


static inline affinity_node_t *get_affinity_info(const copy_opt_t *co, const ir_node *irn)
{
	affinity_node_t find;

	ASSERT_GS_AVAIL(co);

	find.irn = irn;
	return (affinity_node_t*)set_find(co->nodes, &find, sizeof(find), hash_irn(irn));
}

#define co_gs_foreach_aff_node(co, aff_node)     foreach_set((co)->nodes, affinity_node_t, (aff_node))
#define co_gs_foreach_neighb(aff_node, neighb)   for (neighb = aff_node->neighbours; neighb != NULL; neighb = neighb->next)

#endif /* FIRM_BE_BECOPYOPT_T_H */
