#include <cassert>
#include "btorsimam.h"

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

BtorSimBitVector* BtorSimArrayModel::read(const BtorSimBitVector* index)
{
	uint64_t i = btorsim_bv_to_uint64(index);
	assert(i < depth);
	if (!data[i])
	{
		if (random_seed)
			data[i] = btorsim_bv_uint64_to_bv (get_random_init(i), width);
		else
			data[i] = btorsim_bv_new(width);
	}
	return btorsim_bv_copy(data[i]);
}

BtorSimArrayModel* BtorSimArrayModel::write(const BtorSimBitVector* index, const BtorSimBitVector* element)
{
	uint64_t i = btorsim_bv_to_uint64(index);
	assert(i < depth);
	assert(element->width == width);
	BtorSimArrayModel* res = copy();
	if (res->data[i]) btorsim_bv_free(res->data[i]);
	res->data[i] = btorsim_bv_copy(element);
	return res;
}

BtorSimBitVector* BtorSimArrayModel::check(const BtorSimBitVector* index) const
{
	uint64_t i = btorsim_bv_to_uint64(index);
	assert(i < depth);
	if (data.find(i) != data.end())
		return btorsim_bv_copy(data.at(i));
	else
		return nullptr;
}

BtorSimArrayModel* BtorSimArrayModel::copy() const
{
	BtorSimArrayModel* res = new BtorSimArrayModel(width, depth);
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
	assert (a->width == b->width);
	assert (a->depth == b->depth);
	uint32_t bit = (*a == *b) ? 1 : 0;

	BtorSimBitVector* res = btorsim_bv_new(1);
	btorsim_bv_set_bit (res, 0, bit);
	return res;
}

BtorSimBitVector* btorsim_am_neq(const BtorSimArrayModel* a, const BtorSimArrayModel* b)
{
	//TODO: is comparing arrays of different dimensions false or error?
	assert(a), assert(b);
	assert (a->width == b->width);
	assert (a->depth == b->depth);
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
	assert(t->width == e->width);
	assert(t->depth == e->depth);

	BtorSimArrayModel* res;
	if(btorsim_bv_get_bit (c, 0))
		res = t->copy();
	else
		res = e->copy();

	return res;
}