#include <assert.h>
#include "btorsimstate.h"

void BtorSimState::update(BtorSimBitVector *bv)
{
	assert(type == BtorSimStateType::BITVEC);
	if (bv_state) btorsim_bv_free(bv_state);
	bv_state = bv;
}

void BtorSimState::update(BtorSimArrayModel *am)
{
	assert(type == BtorSimStateType::ARRAY);
	if (array_state) delete array_state;
	array_state = am;
}

void BtorSimState::update(BtorSimState& s)
{
	switch (type)
	{
	case BtorSimStateType::ARRAY:
		update(s.array_state);
		break;
	case BtorSimStateType::BITVEC:
		update(s.bv_state);
		break;
	default:
		fprintf (stderr, "*** 'btorsim' error: Updating invalid state!\n");
		exit (1);
	}
}

void BtorSimState::remove()
{
	switch (type)
	{
	case BtorSimStateType::ARRAY:
		if (array_state) delete array_state;
		array_state = nullptr;
		break;
	case BtorSimStateType::BITVEC:
		if (bv_state) btorsim_bv_free(bv_state);
		bv_state = nullptr;
		break;
	default:
		fprintf (stderr, "*** 'btorsim' error: Removing invalid state!\n");
		exit (1);
	}
}

bool BtorSimState::is_set()
{
	switch (type)
	{
	case BtorSimStateType::ARRAY:
		return array_state != nullptr;
	case BtorSimStateType::BITVEC:
		return bv_state != nullptr;\
	default:
		fprintf (stderr, "*** 'btorsim' error: Checking invalid state!\n");
		exit (1);
	}
}
