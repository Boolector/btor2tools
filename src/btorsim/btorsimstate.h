/**
 *  Btor2Tools: A tool package for the BTOR format.
 *
 *  Copyright (c) 2020 Nina Engelhardt.
 *
 *  All rights reserved.
 *
 *  This file is part of the Btor2Tools package.
 *  See LICENSE.txt for more information on using this software.
 */

#ifndef BTOR2STATE_H_INCLUDED
#define BTOR2STATE_H_INCLUDED

#include "btorsimbv.h"
#include "btorsimam.h"

enum BtorSimStateType {INVALID, BITVEC, ARRAY};

struct BtorSimState {
	BtorSimStateType type;
	union {
		BtorSimBitVector *bv_state;
		BtorSimArrayModel *array_state;
	};

	BtorSimState(): type(INVALID), bv_state(nullptr) {};
	void update(BtorSimBitVector *bv);
	void update(BtorSimArrayModel *bv);
	void update(BtorSimState& s);
	void remove();
	bool is_set();
};

#endif
