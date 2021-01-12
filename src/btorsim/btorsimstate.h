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

#ifndef BTOR2STATE_H_INCLUDED
#define BTOR2STATE_H_INCLUDED

#include "btorsimam.h"
#include "btorsimbv.h"

/* Typed union container class for state values, can point to either a BitVector
 * or an ArrayModel. */
struct BtorSimState
{
  enum Type
  {
    INVALID,
    BITVEC,
    ARRAY
  };
  Type type;
  union
  {
    BtorSimBitVector *bv_state;
    BtorSimArrayModel *array_state;
  };

  BtorSimState () : type (INVALID), bv_state (nullptr){};

  /* change the pointed-to value, frees memory of old value
   * does not make a copy of the argument! the input pointer becomes owned by
   * the state
   */
  void update (BtorSimBitVector *bv);
  void update (BtorSimArrayModel *am);
  void update (BtorSimState &s);

  // free memory of value, replace with nullptr
  void remove ();

  // state is not null
  bool is_set ();
};

#endif
