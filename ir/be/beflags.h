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
 * @brief       modifies schedule so flags dependencies are respected.
 * @author      Matthias Braun, Christoph Mallon
 */
#ifndef FIRM_BE_BEFLAGS_H
#define FIRM_BE_BEFLAGS_H

#include "bearch.h"

/**
 * Callback which rematerializes (=duplicates) a machine node.
 */
typedef ir_node * (*func_rematerialize) (ir_node *node, ir_node *after);

/**
 * Callback function that checks whether a node modifies the flags
 */
typedef bool (*check_modifies_flags) (const ir_node *node);

/**
 * Walks the schedule and ensures that flags aren't destroyed between producer
 * and consumer of flags. It does so by moving down/rematerialising of the
 * nodes. This does not work across blocks.
 * The callback functions may be NULL if you want to use default
 * implementations.
 */
void be_sched_fix_flags(ir_graph *irg, const arch_register_class_t *flag_cls,
                        func_rematerialize remat_func,
                        check_modifies_flags check_modifies_flags_func);

#endif
