#include "btorsimbv.h"
#include "btorsimam.h"

struct BtorSimState {
	bool is_array;
	union {
		BtorSimBitVector *bv_state;
		BtorSimArrayModel *array_state;
	};

	BtorSimState(): is_array(false), bv_state(nullptr) {};
	void update(BtorSimBitVector *bv);
	void update(BtorSimArrayModel *bv);
	void update(BtorSimState& s);
	void remove();
	bool is_set();
};
