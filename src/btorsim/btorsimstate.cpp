#include <assert.h>
#include "btorsimstate.h"

void BtorSimState::update(BtorSimBitVector *bv)
{
	assert(!is_array);
	if (bv_state) btorsim_bv_free(bv_state);
	bv_state = bv;
}

void BtorSimState::update(BtorSimArrayModel *am)
{
	assert(is_array);
	if (array_state) delete array_state;
	array_state = am;
}

void BtorSimState::update(BtorSimState& s)
{
	if (s.is_array)
		update(s.bv_state);
	else
		update(s.array_state);
}

void BtorSimState::remove()
{
	if (is_array)
	{
		if (array_state) delete array_state;
		array_state = nullptr;
	}
	else
	{
		if (bv_state) btorsim_bv_free(bv_state);
		bv_state = nullptr;
	}
}

bool BtorSimState::is_set()
{
	if (is_array)
		return array_state != nullptr;
	else
		return bv_state != nullptr;
}
