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
