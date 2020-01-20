#include "btorsimam.h"



BtorSimBitVector* BtorSimArrayModel::read(const BtorSimBitVector* index)
{
	uint64_t i = btorsim_bv_to_uint64(index);
	assert(i < depth);
	//TODO: uninitialized data is assumed to be zero? what about random mode?
	BtorSimBitVector* element = data[i];
	return btorsim_bv_copy(element);
}

BtorSimArrayModel* BtorSimArrayModel::write(const BtorSimBitVector* index, const BtorSimBitVector* element)
{
	uint64_t i = btorsim_bv_to_uint64(index);
	assert(i < depth);
	assert(element->width == width);
	BtorSimArrayModel* res = copy();
	res->data[i] = btorsim_bv_copy(element);
	return res;
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
