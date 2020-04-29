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

struct BtorSimArrayModel {
	uint64_t index_width;
	uint64_t element_width;
	uint64_t random_seed;

	std::unordered_map<std::string, BtorSimBitVector *> data;

	BtorSimArrayModel(uint64_t index_width, uint64_t element_width) : index_width(index_width), element_width(element_width), random_seed(0) {};
	BtorSimArrayModel(uint64_t index_width, uint64_t element_width, uint64_t random_seed) : index_width(index_width), element_width(element_width), random_seed(random_seed) {};
	~BtorSimArrayModel();
	BtorSimArrayModel(const BtorSimArrayModel&) = delete;
	BtorSimArrayModel& operator=(const BtorSimArrayModel&) = delete;

	uint64_t get_random_init(uint64_t idx) const;
	BtorSimBitVector* read(const BtorSimBitVector* index);
	BtorSimArrayModel* write(const BtorSimBitVector* index, const BtorSimBitVector* element);
	BtorSimBitVector* check(const BtorSimBitVector* index) const;
	BtorSimArrayModel* copy() const;
	bool operator!=(const BtorSimArrayModel& other) const;
	bool operator==(const BtorSimArrayModel& other) const;
};

BtorSimBitVector* btorsim_am_eq(const BtorSimArrayModel* a, const BtorSimArrayModel* b);
BtorSimBitVector* btorsim_am_neq(const BtorSimArrayModel* a, const BtorSimArrayModel* b);
BtorSimArrayModel* btorsim_am_ite(const BtorSimBitVector* cond, const BtorSimArrayModel* a, const BtorSimArrayModel* b);

#endif
