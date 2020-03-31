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

#ifndef BTOR2HELP_H_INCLUDED
#define BTOR2HELP_H_INCLUDED

#include <string>
#include <vector>
#include <cstdarg>
#include <cassert>

#include "btor2parser/btor2parser.h"
#include "btorsimbv.h"

extern int32_t verbosity;

void die (const char* m, ...);
void msg (int32_t level, const char* m, ...);

Btor2Sort *get_sort(Btor2Line* l, Btor2Parser *model);
std::string btorsim_bv_to_string (const BtorSimBitVector *bv);
std::string btorsim_bv_to_hex_string (const BtorSimBitVector *bv);

#endif
