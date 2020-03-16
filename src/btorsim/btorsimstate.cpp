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

#include <assert.h>
#include "btorsimstate.h"

void BtorSimState::update(BtorSimBitVector *bv)
{
	assert(type == BtorSimState::Type::BITVEC);
	if (bv_state) btorsim_bv_free(bv_state);
	bv_state = bv;
}

void BtorSimState::update(BtorSimArrayModel *am)
{
	assert(type == ARRAY);
	if (array_state) delete array_state;
	array_state = am;
}

void BtorSimState::update(BtorSimState& s)
{
	switch (type)
	{
	case ARRAY:
		update(s.array_state);
		break;
	case BtorSimState::Type::BITVEC:
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
	case ARRAY:
		if (array_state) delete array_state;
		array_state = nullptr;
		break;
	case BtorSimState::Type::BITVEC:
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
	case ARRAY:
		return array_state != nullptr;
	case BtorSimState::Type::BITVEC:
		return bv_state != nullptr;
	default:
		fprintf (stderr, "*** 'btorsim' error: Checking invalid state!\n");
		exit (1);
	}
}
