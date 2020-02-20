#ifndef BTOR2HELP_H_INCLUDED
#define BTOR2HELP_H_INCLUDED

#include <string>
#include <cstdarg>
#include <cassert>

#include "btor2parser/btor2parser.h"
#include "btorsimbv.h"

extern int32_t verbosity;

void die (const char* m, ...);
void msg (int32_t level, const char* m, ...);

Btor2Sort *get_sort(Btor2Line* l, Btor2Parser *model);
std::string btorsim_bv_to_string (const BtorSimBitVector *bv);

#endif
