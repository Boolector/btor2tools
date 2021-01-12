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

#include "btorsimam.h"

#include <cassert>

#include "btorsimhelpers.h"

BtorSimArrayModel::~BtorSimArrayModel ()
{
  for (auto i : data)
  {
    btorsim_bv_free (i.second);
    data[i.first] = nullptr;
  }
  if (const_init) btorsim_bv_free (const_init);
}

/* Get a 'random' value for an uninitialized element
 * This function must always return the same value for a given index and
 * random_seed. Otherwise reading the same element twice might not give the same
 * value.
 */
uint64_t
BtorSimArrayModel::get_random_init (uint64_t idx) const
{
  return (random_seed + idx) * (random_seed + idx + 1) / 2 + idx;
}

BtorSimBitVector*
BtorSimArrayModel::get_const_init () const
{
  if (const_init)
    return btorsim_bv_copy (const_init);
  else
    return nullptr;
}

BtorSimArrayModel*
BtorSimArrayModel::set_const_init (const BtorSimBitVector* init) const
{
  BtorSimArrayModel* res = copy ();
  if (res->const_init) btorsim_bv_free (res->const_init);
  res->const_init = btorsim_bv_copy (init);
  return res;
}

BtorSimBitVector*
BtorSimArrayModel::read (const BtorSimBitVector* index)
{
  assert (index->width == index_width);
  std::string i = btorsim_bv_to_string (index);
  if (!data[i])
  {
    if (const_init)
      data[i] = btorsim_bv_copy (const_init);
    else if (random_seed)
      data[i] = btorsim_bv_uint64_to_bv (
          get_random_init (btorsim_bv_to_uint64 (index)), element_width);
    else
      data[i] = btorsim_bv_new (element_width);
  }
  return btorsim_bv_copy (data[i]);
}

BtorSimArrayModel*
BtorSimArrayModel::write (const BtorSimBitVector* index,
                          const BtorSimBitVector* element)
{
  assert (index->width == index_width);
  assert (element->width == element_width);
  std::string i          = btorsim_bv_to_string (index);
  BtorSimArrayModel* res = copy ();
  if (res->data[i])
  {
    btorsim_bv_free (res->data[i]);
  }
  res->data[i] = btorsim_bv_copy (element);
  return res;
}

BtorSimBitVector*
BtorSimArrayModel::check (const BtorSimBitVector* index) const
{
  assert (index->width == index_width);
  std::string i = btorsim_bv_to_string (index);
  if (data.find (i) != data.end ())
    return btorsim_bv_copy (data.at (i));
  else
    return nullptr;
}

BtorSimArrayModel*
BtorSimArrayModel::copy () const
{
  BtorSimArrayModel* res = new BtorSimArrayModel (index_width, element_width);
  for (auto i : data) res->data[i.first] = btorsim_bv_copy (i.second);
  if (const_init) res->const_init = btorsim_bv_copy (const_init);
  return res;
}

bool
data_is_subset (const BtorSimArrayModel& self, const BtorSimArrayModel& other)
{
  for (auto i : self.data)
  // check all accessed elements in self have same value in other
  {
    if (other.data.find (i.first) == other.data.end ())
    // data is not in other, but may be same as initial value if an extra read
    // was called on self
    {
      if (other.const_init)  // init value is from init statement
      {
        if (btorsim_bv_compare (i.second, other.const_init)) return false;
      }
      else if (other.random_seed)  // init value is from randomize
      {
        BtorSimBitVector* idx     = btorsim_bv_char_to_bv (i.first.c_str ());
        BtorSimBitVector* initval = btorsim_bv_uint64_to_bv (
            other.get_random_init (btorsim_bv_to_uint64 (idx)),
            other.element_width);
        int is_different_from_random_init =
            btorsim_bv_compare (i.second, initval);
        btorsim_bv_free (idx);
        btorsim_bv_free (initval);
        if (is_different_from_random_init) return false;
      }
      else  // init value is zero
      {
        if (!btorsim_bv_is_zero (i.second)) return false;
      }
    }
    else if (btorsim_bv_compare (other.data.at (i.first), i.second) != 0)
      return false;
  }
  return true;
}

bool
BtorSimArrayModel::operator== (const BtorSimArrayModel& other) const
{
  if (data.size () != ((size_t) 1) << index_width)
  // if all elements were accessed, init values are irrelevant,
  // otherwise they must match
  {
    if (!const_init != !other.const_init)
      return false;  // one initialized but not the other
    if (const_init)
    {
      if (btorsim_bv_compare (const_init, other.const_init) != 0) return false;
    }
    else
    {
      if (random_seed != other.random_seed) return false;
      // when randomize mode is off, two unrelated uninitialized arrays will
      // compare equal, but with randomize they may not. this is ok because
      // both are within the behavior allowed by model and witness.
    }
  }
  // init values match; check accessed data is same
  return data_is_subset (*this, other) && data_is_subset (other, *this);
}

bool
BtorSimArrayModel::operator!= (const BtorSimArrayModel& other) const
{
  return !operator== (other);
}

BtorSimBitVector*
btorsim_am_eq (const BtorSimArrayModel* a, const BtorSimArrayModel* b)
{
  assert (a), assert (b);
  assert (a->element_width == b->element_width);
  assert (a->index_width == b->index_width);
  uint32_t bit = (*a == *b) ? 1 : 0;

  BtorSimBitVector* res = btorsim_bv_new (1);
  btorsim_bv_set_bit (res, 0, bit);
  return res;
}

BtorSimBitVector*
btorsim_am_neq (const BtorSimArrayModel* a, const BtorSimArrayModel* b)
{
  assert (a), assert (b);
  assert (a->element_width == b->element_width);
  assert (a->index_width == b->index_width);
  uint32_t bit = (*a == *b) ? 0 : 1;

  BtorSimBitVector* res = btorsim_bv_new (1);
  btorsim_bv_set_bit (res, 0, bit);
  return res;
}

BtorSimArrayModel*
btorsim_am_ite (const BtorSimBitVector* c,
                const BtorSimArrayModel* t,
                const BtorSimArrayModel* e)
{
  assert (c);
  assert (c->len == 1);
  assert (t);
  assert (e);
  assert (t->element_width == e->element_width);
  assert (t->index_width == e->index_width);

  BtorSimArrayModel* res;
  if (btorsim_bv_get_bit (c, 0))
    res = t->copy ();
  else
    res = e->copy ();

  return res;
}
