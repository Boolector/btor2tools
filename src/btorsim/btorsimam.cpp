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

#include <cassert>
#include "btorsimam.h"
#include "btorsimhelpers.h"

BtorSimArrayModel::~BtorSimArrayModel()
{
	for (auto i: data)
	{
		btorsim_bv_free(i.second);
		data[i.first] = nullptr;
	}
}

uint64_t BtorSimArrayModel::get_random_init(uint64_t idx) const
{
	return (random_seed + idx)*(random_seed + idx + 1)/2 + idx;
}

BtorSimBitVector* BtorSimArrayModel::read (const BtorSimBitVector* index)
{
	assert (index->width <= index_width);
	std::string i = btorsim_bv_to_string(index);
	if (!data[i])
	{
		if (random_seed)
			data[i] = btorsim_bv_uint64_to_bv (get_random_init(btorsim_bv_to_uint64(index)), element_width);
		else
			data[i] = btorsim_bv_new (element_width);
	}
	return btorsim_bv_copy (data[i]);
}

BtorSimArrayModel* BtorSimArrayModel::write(const BtorSimBitVector* index, const BtorSimBitVector* element)
{
	assert (index->width <= index_width);
	assert (element->width == element_width);
	std::string i = btorsim_bv_to_string (index);
	BtorSimArrayModel* res = copy();
	if (res->data[i]) {
		btorsim_bv_free (res->data[i]);
	}
	res->data[i] = btorsim_bv_copy (element);
	return res;
}

BtorSimBitVector* BtorSimArrayModel::check(const BtorSimBitVector* index) const
{
	assert (index->width <= index_width);
	std::string i = btorsim_bv_to_string(index);
	if (data.find(i) != data.end())
		return btorsim_bv_copy(data.at(i));
	else
		return nullptr;
}

BtorSimArrayModel* BtorSimArrayModel::copy() const
{
	BtorSimArrayModel* res = new BtorSimArrayModel(index_width, element_width);
	for (auto i: data)
		res->data[i.first] = btorsim_bv_copy(i.second);
	return res;
}

bool BtorSimArrayModel::operator==(const BtorSimArrayModel& other) const
{
	for (auto i: data)
		if (other.data.find(i.first)==other.data.end() || btorsim_bv_compare(other.data.at(i.first), i.second) != 0)
		{
			return false;
		}
	return true;
}

bool BtorSimArrayModel::operator!=(const BtorSimArrayModel& other) const
{
	return !operator==(other);
}



BtorSimBitVector* btorsim_am_eq(const BtorSimArrayModel* a, const BtorSimArrayModel* b)
{
	//TODO: is comparing arrays of different dimensions false or error?
	assert (a), assert (b);
	assert (a->element_width == b->element_width);
	assert (a->index_width == b->index_width);
	uint32_t bit = (*a == *b) ? 1 : 0;

	BtorSimBitVector* res = btorsim_bv_new(1);
	btorsim_bv_set_bit (res, 0, bit);
	return res;
}

BtorSimBitVector* btorsim_am_neq(const BtorSimArrayModel* a, const BtorSimArrayModel* b)
{
	//TODO: is comparing arrays of different dimensions false or error?
	assert(a), assert(b);
	assert (a->element_width == b->element_width);
	assert (a->index_width == b->index_width);
	uint32_t bit = (*a == *b) ? 0 : 1;

	BtorSimBitVector* res = btorsim_bv_new(1);
	btorsim_bv_set_bit (res, 0, bit);
	return res;
}

BtorSimArrayModel* btorsim_am_ite(const BtorSimBitVector* c, const BtorSimArrayModel* t, const BtorSimArrayModel* e)
{
	assert (c);
	assert (c->len == 1);
	assert(t);
	assert(e);
	assert(t->element_width == e->element_width);
	assert(t->index_width == e->index_width);

	BtorSimArrayModel* res;
	if(btorsim_bv_get_bit (c, 0))
		res = t->copy();
	else
		res = e->copy();

	return res;
}
