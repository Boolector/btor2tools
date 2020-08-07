/**
 *  Btor2Tools: A tool package for the BTOR format.
 *
 *  Copyright (c) 2013-2016 Mathias Preiner.
 *  Copyright (c) 2015-2018 Aina Niemetz.
 *  Copyright (c) 2018 Armin Biere.
 *  Copyright (c) 2020 Nina Engelhardt.
 *
 *  All rights reserved.
 *
 *  This file is part of the Btor2Tools package.
 *  See LICENSE.txt for more information on using this software.
 */

#ifndef BTOR2AM_H_INCLUDED
#define BTOR2AM_H_INCLUDED

#include <string>
#include <unordered_map>

#include "btorsimbv.h"

struct BtorSimArrayModel
{
  /* length of the array index sort */
  uint64_t index_width = 0;

  /* length of the array element sort */
  uint64_t element_width = 0;

  /* if not 0, uninitialized array elements will have a pseudo-random value */
  uint64_t random_seed = 0;

  /* if not null, global array initialization value from a init <array> <vector>
   * statement */
  BtorSimBitVector* const_init = nullptr;

  /* Only the values of previously accessed memory elements are stored. Indexes
   * are represented as strings of '0' and '1' because they can represent
   * arbitrarily large vectors, have a well-defined length, and do not require
   * custom comparison functions. If a not previously accessed element is read,
   * an entry is created and populated with the first existing value of:
   * - const_init (if the entire array has been initialized with a vector)
   * - a reproducible pseudo-random value provided by get_random_init based on
   * index value and random_seed (if non-zero, i.e. when randomize mode is
   * enabled)
   * - zero.
   * It is important to populate on read and not only on write so that a full
   * account of all accessed memory elements and their values is shown in the
   * trace.
   */
  std::unordered_map<std::string, BtorSimBitVector*> data;

  BtorSimArrayModel (uint64_t index_width, uint64_t element_width)
      : index_width (index_width), element_width (element_width){};
  BtorSimArrayModel (uint64_t index_width,
                     uint64_t element_width,
                     uint64_t random_seed)
      : index_width (index_width),
        element_width (element_width),
        random_seed (random_seed){};
  ~BtorSimArrayModel ();

  /* avoid accidental copies */
  BtorSimArrayModel (const BtorSimArrayModel&) = delete;
  BtorSimArrayModel& operator= (const BtorSimArrayModel&) = delete;

  /* obtain a pseudo-random value for an uninitialized element (always the same
   * for given index and random_seed) */
  uint64_t get_random_init (uint64_t idx) const;

  /* get a copy of the global array init value */
  BtorSimBitVector* get_const_init () const;

  /* set the global array init value (copies the *init argument vector, does not
   * take ownership */
  BtorSimArrayModel* set_const_init (const BtorSimBitVector* init) const;

  /* obtain a copy of the element at index, create entry if not previously
   * accessed and initialize appropriately */
  BtorSimBitVector* read (const BtorSimBitVector* index);

  /* return a copy of the array with the element written at index (copies the
   * *element argument vector, does not take ownership) */
  BtorSimArrayModel* write (const BtorSimBitVector* index,
                            const BtorSimBitVector* element);

  /* obtain a copy of the element at index only if it was already previously
   * accessed, return null otherwise */
  BtorSimBitVector* check (const BtorSimBitVector* index) const;

  /* return a copy of the array */
  BtorSimArrayModel* copy () const;

  /* equality checks test for:
   * - same global init (unless all elements were already accessed)
   * - same random seed (unless all elements were already accessed)
   * - same element values at accessed indices (or equal to init value if
   * accessed only in one: this may happen if one copy has an extra read()
   * called on it)
   */
  bool operator!= (const BtorSimArrayModel& other) const;
  bool operator== (const BtorSimArrayModel& other) const;
};

/* Array variants of polymorphic operators that also exist on vectors */
BtorSimBitVector* btorsim_am_eq (const BtorSimArrayModel* a,
                                 const BtorSimArrayModel* b);
BtorSimBitVector* btorsim_am_neq (const BtorSimArrayModel* a,
                                  const BtorSimArrayModel* b);
BtorSimArrayModel* btorsim_am_ite (const BtorSimBitVector* cond,
                                   const BtorSimArrayModel* a,
                                   const BtorSimArrayModel* b);

#endif
