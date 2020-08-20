/**
 *  Btor2Tools: A tool package for the BTOR format.
 *
 *  Copyright (c) 2018 Armin Biere.
 *  Copyright (c) 2018 Aina Niemetz.
 *  Copyright (c) 2019 Mathias Preiner.
 *  Copyright (c) 2020 Nina Engelhardt.
 *
 *  All rights reserved.
 *
 *  This file is part of the Btor2Tools package.
 *  See LICENSE.txt for more information on using this software.
 */

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "btor2parser/btor2parser.h"
#include "btorsimbv.h"
#include "btorsimrng.h"
#include "btorsimstate.h"
#include "btorsimvcd.h"

/*------------------------------------------------------------------------*/

int32_t verbosity;
static bool print_states;

static const char *usage =
    "usage: btorsim [ <option> ... ] [ <btor> [ <witness> ] ]\n"
    "\n"
    "where <option> is one of the following\n"
    "\n"
    "  -h        print this command line option summary\n"
    "  -c        check only <witness> and do not print trace\n"
    "  -v        increase verbosity level (multiple times if necessary)\n"
    "  -r <n>    generate <n> random transitions (default 20)\n"
    "  -s <s>    random seed (default '0')\n"
    "\n"
    "  -b <n>    fake simulation to satisfy bad state property 'b<n>'\n"
    "  -j <n>    fake simulation to satisfy justice property 'j<n>'\n"
    "\n"
    "  --states                 print all states\n"
    "  --vcd <file>             write VCD trace to <file>\n"
    "  --hierarchical-symbols   interpret '.' in symbol names as hierarchical\n"
    "                           module path in VCD\n"
    "  --info <file>            read additional information for clocks and\n"
    "                           top module name from <file>\n"
    "\n"
    "and '<btor>' is sequential model in 'BTOR' format\n"
    "and '<witness>' a trace in 'BTOR' witness format.\n"
    "\n"
    "The simulator either checks a given witness (checking mode) or\n"
    "randomly generates inputs (random mode). If no BTOR model path is\n"
    "specified then it is read from '<stdin>'.  The simulator only uses\n"
    "checking mode if both the BTOR model and a witness file are specified.\n";

static const char *model_path;
static const char *witness_path;
static FILE *model_file;
static FILE *witness_file;
static int32_t close_model_file;
static int32_t close_witness_file;

static bool dump_vcd   = false;
static bool symbol_fmt = false;
#ifdef NDEBUG
static const bool readable_vcd = false;
#else
static const bool readable_vcd = true;
#endif

static const char *vcd_path;
static BtorSimVCDWriter *vcd_writer;

static int32_t
parse_int (const char *str, int32_t *res_ptr)
{
  const char *p = str;
  if (!*p) return 0;
  if (*p == '0' && p[1]) return 0;
  int32_t res = 0;
  while (*p)
  {
    const int32_t ch = *p++;
    if (!isdigit (ch)) return 0;
    if (INT_MAX / 10 < res) return 0;
    res *= 10;
    const int32_t digit = ch - '0';
    if (INT_MAX - digit < res) return 0;
    res += digit;
  }
  *res_ptr = res;
  return 1;
}

static int32_t
parse_long (const char *str, int64_t *res_ptr)
{
  const char *p = str;
  if (!*p) return 0;
  if (*p == '0' && p[1]) return 0;
  int64_t res = 0;
  while (*p)
  {
    const int32_t ch = *p++;
    if (!isdigit (ch)) return 0;
    if (LONG_MAX / 10 < res) return 0;
    res *= 10;
    const int32_t digit = ch - '0';
    if (LONG_MAX - digit < res) return 0;
    res += digit;
  }
  *res_ptr = res;
  return 1;
}

static int32_t checking_mode = 0;
static int32_t random_mode   = 0;

static Btor2Parser *model;

static std::vector<Btor2Line *> inputs;
static std::vector<Btor2Line *> states;
static std::vector<Btor2Line *> bads;
static std::vector<Btor2Line *> constraints;
static std::vector<Btor2Line *> justices;

static std::vector<int64_t> reached_bads;

static int64_t constraints_violated = -1;
static int64_t num_unreached_bads;

static std::map<int64_t, std::string> extra_constraints;

static int64_t num_format_lines;
static std::vector<Btor2Line *> inits;
static std::vector<Btor2Line *> nexts;

static std::vector<BtorSimState> current_state;
static std::vector<BtorSimState> next_state;

static void
parse_model_line (Btor2Line *l)
{
  switch (l->tag)
  {
    case BTOR2_TAG_bad: {
      int64_t i = (int64_t) bads.size ();
      msg (2, "bad %" PRId64 " at line %" PRId64, i, l->lineno);
      bads.push_back (l);
      reached_bads.push_back (-1);
      num_unreached_bads++;
    }
    break;

    case BTOR2_TAG_constraint: {
      int64_t i = (int64_t) constraints.size ();
      msg (2, "constraint %" PRId64 " at line %" PRId64, i, l->lineno);
      constraints.push_back (l);
    }
    break;

    case BTOR2_TAG_init: inits[l->args[0]] = l; break;

    case BTOR2_TAG_input: {
      int64_t i = (int64_t) inputs.size ();
      if (l->symbol)
        msg (2,
             "input %" PRId64 " '%s' at line %" PRId64,
             i,
             l->symbol,
             l->lineno);
      else
        msg (2, "input %" PRId64 " at line %" PRId64, i, l->lineno);
      inputs.push_back (l);
    }
    break;

    case BTOR2_TAG_next: nexts[l->args[0]] = l; break;

    case BTOR2_TAG_sort: {
      switch (l->sort.tag)
      {
        case BTOR2_TAG_SORT_bitvec:
          msg (2,
               "sort bitvec %u at line %" PRId64,
               l->sort.bitvec.width,
               l->lineno);
          break;
        case BTOR2_TAG_SORT_array:
          msg (2,
               "sort array %u %u at line %" PRId64,
               l->sort.array.index,
               l->sort.array.element,
               l->lineno);
          break;
        default:
          die ("parse error in '%s' at line %" PRId64 ": unsupported sort '%s'",
               model_path,
               l->lineno,
               l->sort.name);
          break;
      }
    }
    break;

    case BTOR2_TAG_state: {
      int64_t i = (int64_t) states.size ();
      if (l->symbol)
      {
        msg (2,
             "state %" PRId64 " '%s' at line %" PRId64,
             i,
             l->symbol,
             l->lineno);
      }
      else
      {
        msg (2, "state %" PRId64 " at line %" PRId64, i, l->lineno);
      }
      states.push_back (l);
    }
    break;

    case BTOR2_TAG_add:
    case BTOR2_TAG_and:
    case BTOR2_TAG_concat:
    case BTOR2_TAG_const:
    case BTOR2_TAG_constd:
    case BTOR2_TAG_consth:
    case BTOR2_TAG_dec:
    case BTOR2_TAG_eq:
    case BTOR2_TAG_implies:
    case BTOR2_TAG_inc:
    case BTOR2_TAG_ite:
    case BTOR2_TAG_mul:
    case BTOR2_TAG_nand:
    case BTOR2_TAG_neg:
    case BTOR2_TAG_neq:
    case BTOR2_TAG_nor:
    case BTOR2_TAG_not:
    case BTOR2_TAG_one:
    case BTOR2_TAG_ones:
    case BTOR2_TAG_or:
    case BTOR2_TAG_output:
    case BTOR2_TAG_redand:
    case BTOR2_TAG_redor:
    case BTOR2_TAG_redxor:
    case BTOR2_TAG_sdiv:
    case BTOR2_TAG_sext:
    case BTOR2_TAG_sgt:
    case BTOR2_TAG_sgte:
    case BTOR2_TAG_slice:
    case BTOR2_TAG_sll:
    case BTOR2_TAG_slt:
    case BTOR2_TAG_slte:
    case BTOR2_TAG_sra:
    case BTOR2_TAG_srem:
    case BTOR2_TAG_srl:
    case BTOR2_TAG_sub:
    case BTOR2_TAG_udiv:
    case BTOR2_TAG_uext:
    case BTOR2_TAG_ugt:
    case BTOR2_TAG_ugte:
    case BTOR2_TAG_ult:
    case BTOR2_TAG_ulte:
    case BTOR2_TAG_urem:
    case BTOR2_TAG_xnor:
    case BTOR2_TAG_xor:
    case BTOR2_TAG_zero:
    case BTOR2_TAG_read:
    case BTOR2_TAG_write: break;

    case BTOR2_TAG_fair:
    case BTOR2_TAG_justice:
    case BTOR2_TAG_rol:
    case BTOR2_TAG_ror:
    case BTOR2_TAG_saddo:
    case BTOR2_TAG_sdivo:
    case BTOR2_TAG_smod:
    case BTOR2_TAG_smulo:
    case BTOR2_TAG_ssubo:
    case BTOR2_TAG_uaddo:
    case BTOR2_TAG_umulo:
    case BTOR2_TAG_usubo:
    default:
      die ("parse error in '%s' at line %" PRId64 ": unsupported '%" PRId64
           " %s%s'",
           model_path,
           l->lineno,
           l->id,
           l->name,
           l->nargs ? " ..." : "");
      break;
  }
}

static void
parse_model ()
{
  assert (model_file);
  model = btor2parser_new ();
  if (!btor2parser_read_lines (model, model_file))
    die ("parse error in '%s' at %s", model_path, btor2parser_error (model));
  num_format_lines = btor2parser_max_id (model);
  inits.resize (num_format_lines, nullptr);
  nexts.resize (num_format_lines, nullptr);
  Btor2LineIterator it = btor2parser_iter_init (model);
  Btor2Line *line;
  while ((line = btor2parser_iter_next (&it))) parse_model_line (line);

  for (size_t i = 0; i < states.size (); i++)
  {
    Btor2Line *state = states[i];
    if (!nexts[state->id])
    {
      msg (1, "state %d without next function", state->id);
    }
  }
}

static void
update_current_state (int64_t id, BtorSimBitVector *bv)
{
  assert (0 <= id), assert (id < num_format_lines);
  msg (5, "updating state %" PRId64, id);
  current_state[id].update (bv);
}

static void
update_current_state (int64_t id, BtorSimArrayModel *am)
{
  assert (0 <= id), assert (id < num_format_lines);
  msg (5, "updating state %" PRId64, id);
  current_state[id].update (am);
}

static void
update_current_state (int64_t id, BtorSimState &s)
{
  assert (0 <= id), assert (id < num_format_lines);
  msg (5, "updating state %" PRId64, id);
  current_state[id].update (s);
}

static void
delete_current_state (int64_t id)
{
  assert (0 <= id), assert (id < num_format_lines);
  if (current_state[id].type) current_state[id].remove ();
}

static BtorSimState
simulate (int64_t id)
{
  int32_t sign = id < 0 ? -1 : 1;
  if (sign < 0) id = -id;
  assert (0 <= id), assert (id < num_format_lines);
  BtorSimState res = current_state[id];
  if (!res.is_set ())
  {
    Btor2Line *l = btor2parser_get_line_by_id (model, id);
    if (!l) die ("internal error: unexpected empty ID %" PRId64, id);
    BtorSimState args[3];
    for (uint32_t i = 0; i < l->nargs; i++) args[i] = simulate (l->args[i]);
    switch (l->tag)
    {
      case BTOR2_TAG_add:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_add (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_and:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_and (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_concat:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_concat (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_const:
        assert (l->nargs == 0);
        assert (res.type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_char_to_bv (l->constant);
        break;
      case BTOR2_TAG_constd:
        assert (l->nargs == 0);
        assert (res.type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_constd (l->constant, l->sort.bitvec.width);
        break;
      case BTOR2_TAG_consth:
        assert (l->nargs == 0);
        assert (res.type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_consth (l->constant, l->sort.bitvec.width);
        break;
      case BTOR2_TAG_dec:
        assert (l->nargs == 1);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_dec (args[0].bv_state);
        break;
      case BTOR2_TAG_eq:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        if (args[0].type == BtorSimState::Type::ARRAY)
        {
          assert (args[1].type == BtorSimState::Type::ARRAY);
          res.bv_state =
              btorsim_am_eq (args[0].array_state, args[1].array_state);
        }
        else
        {
          assert (args[0].type == BtorSimState::Type::BITVEC);
          assert (args[1].type == BtorSimState::Type::BITVEC);
          res.bv_state = btorsim_bv_eq (args[0].bv_state, args[1].bv_state);
        }
        break;
      case BTOR2_TAG_implies:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_implies (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_inc:
        assert (l->nargs == 1);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_inc (args[0].bv_state);
        break;
      case BTOR2_TAG_ite:
        assert (l->nargs == 3);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        if (res.type == BtorSimState::Type::ARRAY)
        {
          assert (args[1].type == BtorSimState::Type::ARRAY);
          assert (args[2].type == BtorSimState::Type::ARRAY);
          res.array_state = btorsim_am_ite (
              args[0].bv_state, args[1].array_state, args[2].array_state);
        }
        else
        {
          assert (args[1].type == BtorSimState::Type::BITVEC);
          assert (args[2].type == BtorSimState::Type::BITVEC);
          res.bv_state = btorsim_bv_ite (
              args[0].bv_state, args[1].bv_state, args[2].bv_state);
        }
        break;
      case BTOR2_TAG_mul:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_mul (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_nand:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_nand (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_neg:
        assert (l->nargs == 1);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_neg (args[0].bv_state);
        break;
      case BTOR2_TAG_neq:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        if (args[0].type == BtorSimState::Type::ARRAY)
        {
          assert (args[1].type == BtorSimState::Type::ARRAY);
          res.bv_state =
              btorsim_am_neq (args[0].array_state, args[1].array_state);
        }
        else
        {
          assert (args[0].type == BtorSimState::Type::BITVEC);
          assert (args[1].type == BtorSimState::Type::BITVEC);
          res.bv_state = btorsim_bv_neq (args[0].bv_state, args[1].bv_state);
        }
        break;
      case BTOR2_TAG_nor:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_nor (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_not:
        assert (l->nargs == 1);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_not (args[0].bv_state);
        break;
      case BTOR2_TAG_one:
        assert (res.type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_one (l->sort.bitvec.width);
        break;
      case BTOR2_TAG_ones:
        assert (res.type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_ones (l->sort.bitvec.width);
        break;
      case BTOR2_TAG_or:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_or (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_redand:
        assert (l->nargs == 1);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_redand (args[0].bv_state);
        break;
      case BTOR2_TAG_redor:
        assert (l->nargs == 1);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_redor (args[0].bv_state);
        break;
      case BTOR2_TAG_redxor:
        assert (l->nargs == 1);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_redxor (args[0].bv_state);
        break;
      case BTOR2_TAG_slice:
        assert (l->nargs == 1);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        res.bv_state =
            btorsim_bv_slice (args[0].bv_state, l->args[1], l->args[2]);
        break;
      case BTOR2_TAG_sub:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_sub (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_uext:
        assert (l->nargs == 1);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        {
          uint32_t width = args[0].bv_state->width;
          assert (width <= l->sort.bitvec.width);
          uint32_t padding = l->sort.bitvec.width - width;
          if (padding)
            res.bv_state = btorsim_bv_uext (args[0].bv_state, padding);
          else
            res.bv_state = btorsim_bv_copy (args[0].bv_state);
        }
        break;
      case BTOR2_TAG_udiv:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_udiv (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_sdiv:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_sdiv (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_sext:
        assert (l->nargs == 1);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        {
          uint32_t width = args[0].bv_state->width;
          assert (width <= l->sort.bitvec.width);
          uint32_t padding = l->sort.bitvec.width - width;
          if (padding)
            res.bv_state = btorsim_bv_sext (args[0].bv_state, padding);
          else
            res.bv_state = btorsim_bv_copy (args[0].bv_state);
        }
        break;
      case BTOR2_TAG_sll:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_sll (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_srl:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_srl (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_sra:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_sra (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_srem:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_srem (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_ugt:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_ult (args[1].bv_state, args[0].bv_state);
        break;
      case BTOR2_TAG_ugte:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_ulte (args[1].bv_state, args[0].bv_state);
        break;
      case BTOR2_TAG_ult:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC),
            assert (args[0].type == BtorSimState::Type::BITVEC),
            assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_ult (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_ulte:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_ulte (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_urem:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_urem (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_sgt:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_slt (args[1].bv_state, args[0].bv_state);
        break;
      case BTOR2_TAG_sgte:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_slte (args[1].bv_state, args[0].bv_state);
        break;
      case BTOR2_TAG_slt:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_slt (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_slte:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_slte (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_iff:
      case BTOR2_TAG_xnor:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_xnor (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_xor:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::BITVEC);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_xor (args[0].bv_state, args[1].bv_state);
        break;
      case BTOR2_TAG_zero:
        assert (res.type == BtorSimState::Type::BITVEC);
        res.bv_state = btorsim_bv_zero (l->sort.bitvec.width);
        break;
      case BTOR2_TAG_read:
        assert (l->nargs == 2);
        assert (res.type == BtorSimState::Type::BITVEC);
        assert (args[0].type == BtorSimState::Type::ARRAY);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        res.bv_state = args[0].array_state->read (args[1].bv_state);
        {
          Btor2Line *mem = btor2parser_get_line_by_id (model, l->args[0]);
          msg (4,
               "read %s[%s] -> %s",
               mem->symbol ? mem->symbol : std::to_string (mem->id).c_str (),
               btorsim_bv_to_string (args[1].bv_state).c_str (),
               btorsim_bv_to_string (res.bv_state).c_str ());
        }
        break;
      case BTOR2_TAG_write:
        assert (l->nargs == 3);
        assert (res.type == BtorSimState::Type::ARRAY);
        assert (args[0].type == BtorSimState::Type::ARRAY);
        assert (args[1].type == BtorSimState::Type::BITVEC);
        assert (args[2].type == BtorSimState::Type::BITVEC);
        res.array_state =
            args[0].array_state->write (args[1].bv_state, args[2].bv_state);
        {
          Btor2Line *mem = btor2parser_get_line_by_id (model, l->args[0]);
          msg (4,
               "write %s[%s] <- %s",
               mem->symbol ? mem->symbol : std::to_string (mem->id).c_str (),
               btorsim_bv_to_string (args[1].bv_state).c_str (),
               btorsim_bv_to_string (args[2].bv_state).c_str ());
        }
        break;
      default:
        die ("can not randomly simulate operator '%s' at line %" PRId64,
             l->name,
             l->lineno);
        break;
    }
    for (uint32_t i = 0; i < l->nargs; i++) args[i].remove ();
    update_current_state (id, res);
  }
  if (res.type == BtorSimState::Type::ARRAY)
  {
    res.array_state = res.array_state->copy ();
  }
  else
  {
    assert (res.type == BtorSimState::Type::BITVEC);
    if (sign < 0)
      res.bv_state = btorsim_bv_not (res.bv_state);
    else
      res.bv_state = btorsim_bv_copy (res.bv_state);
  }
  return res;
}

static bool print_trace = true;
static BtorSimRNG rng;

static void
print_state_or_input (int64_t id, int64_t pos, int64_t step, bool is_input)
{
  Btor2Line *l = btor2parser_get_line_by_id (model, id);
  switch (current_state[id].type)
  {
    case BtorSimState::Type::BITVEC:
      printf ("%" PRId64 " ", pos);
      btorsim_bv_print_without_new_line (current_state[id].bv_state);
      if (l->symbol)
        printf (" %s%s%" PRId64, l->symbol, is_input ? "@" : "#", step);
      fputc ('\n', stdout);
      break;
    case BtorSimState::Type::ARRAY:
      for (auto e : current_state[id].array_state->data)
      {
        printf ("%" PRId64 " [%s]", pos, e.first.c_str ());
        btorsim_bv_print_without_new_line (e.second);
        if (l->symbol)
          printf (" %s%s%" PRId64, l->symbol, is_input ? "@" : "#", step);
        fputc ('\n', stdout);
      }
      break;
    default: die ("uninitialized current_state %" PRId64, id);
  }
}

static void
initialize_inputs (int64_t k, int32_t randomize)
{
  msg (1, "initializing inputs @%" PRId64, k);
  if (print_trace) printf ("@%" PRId64 "\n", k);
  for (size_t i = 0; i < inputs.size (); i++)
  {
    Btor2Line *input = inputs[i];
    if (!current_state[input->id].is_set ())
    // if not set previously by parse_input_part from witness
    {
      if (input->sort.tag == BTOR2_TAG_SORT_bitvec)
      {
        uint32_t width = input->sort.bitvec.width;
        BtorSimBitVector *update;
        if (randomize)
          update = btorsim_bv_new_random (&rng, width);
        else
          update = btorsim_bv_new (width);
        update_current_state (input->id, update);
      }
      else
      {
        assert (input->sort.tag == BTOR2_TAG_SORT_array);
        Btor2Line *li =
            btor2parser_get_line_by_id (model, input->sort.array.index);
        Btor2Line *le =
            btor2parser_get_line_by_id (model, input->sort.array.element);
        assert (li->sort.tag == BTOR2_TAG_SORT_bitvec);
        assert (le->sort.tag == BTOR2_TAG_SORT_bitvec);
        BtorSimArrayModel *am = new BtorSimArrayModel (li->sort.bitvec.width,
                                                       le->sort.bitvec.width);
        if (randomize)
        {
          am->random_seed = btorsim_rng_rand (&rng);
        }
        update_current_state (input->id, am);
      }
    }
    if (print_trace) print_state_or_input (input->id, i, k, true);
  }
}

static void
initialize_states (int32_t randomly)
{
  msg (1, "initializing states at #0");
  if (print_trace) printf ("#0\n");
  for (size_t i = 0; i < states.size (); i++)
  {
    Btor2Line *state = states[i];
    assert (0 <= state->id), assert (state->id < num_format_lines);
    Btor2Line *init = inits[state->id];
    if (!current_state[state->id].is_set ())
    {  // can be set in parse_state_part from witness
      switch (current_state[state->id].type)
      {
        case BtorSimState::Type::BITVEC: {
          assert (state->sort.tag == BTOR2_TAG_SORT_bitvec);
          if (init)
          {
            assert (init->nargs == 2);
            assert (init->args[0] == state->id);
            BtorSimState update = simulate (init->args[1]);
            assert (update.type == BtorSimState::Type::BITVEC);
            update_current_state (state->id, update);
          }
          else
          {
            BtorSimBitVector *bv;
            if (randomly)
              bv = btorsim_bv_new_random (&rng, state->sort.bitvec.width);
            else
              bv = btorsim_bv_new (state->sort.bitvec.width);
            update_current_state (state->id, bv);
          }
        }
        break;
        case BtorSimState::Type::ARRAY:
          assert (state->sort.tag == BTOR2_TAG_SORT_array);
          if (init)
          {
            assert (init->nargs == 2);
            assert (init->args[0] == state->id);
            BtorSimState update = simulate (init->args[1]);
            switch (update.type)
            {
              case BtorSimState::Type::ARRAY:
                update_current_state (state->id, update);
                break;
              case BtorSimState::Type::BITVEC: {
                Btor2Line *li =
                    btor2parser_get_line_by_id (model, state->sort.array.index);
                Btor2Line *le = btor2parser_get_line_by_id (
                    model, state->sort.array.element);
                assert (li->sort.tag == BTOR2_TAG_SORT_bitvec);
                assert (le->sort.tag == BTOR2_TAG_SORT_bitvec);
                BtorSimArrayModel *am = new BtorSimArrayModel (
                    li->sort.bitvec.width, le->sort.bitvec.width);
                am->const_init = update.bv_state;
                update_current_state (state->id, am);
              }
              break;
              default: die ("bad result simulating %" PRId64, init->args[1]);
            }
          }
          else
          {
            Btor2Line *li =
                btor2parser_get_line_by_id (model, state->sort.array.index);
            Btor2Line *le =
                btor2parser_get_line_by_id (model, state->sort.array.element);
            assert (li->sort.tag == BTOR2_TAG_SORT_bitvec);
            assert (le->sort.tag == BTOR2_TAG_SORT_bitvec);
            BtorSimArrayModel *am = new BtorSimArrayModel (
                li->sort.bitvec.width, le->sort.bitvec.width);
            if (randomly)
            {
              am->random_seed = btorsim_rng_rand (&rng);
            }
            update_current_state (state->id, am);
          }
          break;
        default: die ("uninitialized current_state %" PRId64, state->id);
      }
    }
    if (print_trace && !init) print_state_or_input (state->id, i, 0, false);
  }
}

static void
simulate_step (int64_t k, int32_t randomize_states_that_are_inputs)
{
  msg (1, "simulating step %" PRId64, k);
  for (int64_t i = 0; i < num_format_lines; i++)
  {
    Btor2Line *l = btor2parser_get_line_by_id (model, i);
    if (!l) continue;
    if (l->tag == BTOR2_TAG_sort || l->tag == BTOR2_TAG_init
        || l->tag == BTOR2_TAG_next || l->tag == BTOR2_TAG_bad
        || l->tag == BTOR2_TAG_constraint || l->tag == BTOR2_TAG_fair
        || l->tag == BTOR2_TAG_justice || l->tag == BTOR2_TAG_output)
      continue;

    BtorSimState s = simulate (i);
#if 0
    printf ("[btorim] %" PRId64 " %s ", l->id, l->name);
    if(s.type == BtorSimState::Type::BITVEC) btorsim_bv_print (s.bv_state);
    fflush (stdout);
#endif
    s.remove ();
  }
  for (size_t i = 0; i < states.size (); i++)
  {
    Btor2Line *state = states[i];
    assert (0 <= state->id), assert (state->id < num_format_lines);
    Btor2Line *next = nexts[state->id];
    BtorSimState update;
    if (next)
    {
      assert (next->nargs == 2);
      assert (next->args[0] == state->id);
      update = simulate (next->args[1]);
    }
    else
    {
      if (state->sort.tag == BTOR2_TAG_SORT_bitvec)
      {
        update.type    = BtorSimState::Type::BITVEC;
        uint32_t width = state->sort.bitvec.width;
        if (randomize_states_that_are_inputs)
          update.bv_state = btorsim_bv_new_random (&rng, width);
        else
          update.bv_state = btorsim_bv_new (width);
      }
      else
      {
        assert (state->sort.tag == BTOR2_TAG_SORT_array);
        update.type = BtorSimState::Type::ARRAY;
        Btor2Line *li =
            btor2parser_get_line_by_id (model, state->sort.array.index);
        Btor2Line *le =
            btor2parser_get_line_by_id (model, state->sort.array.element);
        assert (li->sort.tag == BTOR2_TAG_SORT_bitvec);
        assert (le->sort.tag == BTOR2_TAG_SORT_bitvec);
        update.array_state = new BtorSimArrayModel (li->sort.bitvec.width,
                                                    le->sort.bitvec.width);
        if (randomize_states_that_are_inputs)
          update.array_state->random_seed = btorsim_rng_rand (&rng);
      }
    }
    assert (!next_state[state->id].is_set ());
    assert (next_state[state->id].type == update.type);
    next_state[state->id] = update;
  }

  if (constraints_violated < 0)
  {
    for (size_t i = 0; i < constraints.size (); i++)
    {
      Btor2Line *constraint = constraints[i];
      BtorSimState s        = current_state[constraint->args[0]];
      assert (s.type == BtorSimState::Type::BITVEC);
      if (!btorsim_bv_is_zero (s.bv_state)) continue;
      msg (1,
           "constraint(%" PRId64 ") '%" PRId64 " constraint %" PRId64
           "' violated at time %" PRId64,
           i,
           constraint->id,
           constraint->args[0],
           k);
      constraints_violated = k;
    }
  }

  if (constraints_violated < 0)
  {
    for (size_t i = 0; i < bads.size (); i++)
    {
      int64_t r = reached_bads[i];
      if (r >= 0) continue;
      Btor2Line *bad = bads[i];
      BtorSimState s = current_state[bad->args[0]];
      assert (s.type == BtorSimState::Type::BITVEC);
      if (btorsim_bv_is_zero (s.bv_state)) continue;
      int64_t bound = reached_bads[i];
      if (bound >= 0) continue;
      reached_bads[i] = k;
      assert (num_unreached_bads > 0);
      if (!--num_unreached_bads)
        msg (1,
             "all %" PRId64 " bad state properties reached",
             (int64_t) bads.size ());
    }
  }

  /* Check additional bads from info file */
  for (auto it : extra_constraints)
  {
    BtorSimState s = current_state[it.first];
    assert (s.type == BtorSimState::Type::BITVEC);
    if (btorsim_bv_is_zero (s.bv_state)) continue;
    printf ("[btorsim] Assert failed in test: %s (step %" PRId64 ")\n",
            it.second.c_str (),
            k);
  }

  if (dump_vcd)
  {
    for (int i = 0; i < num_format_lines; i++)
    {
      Btor2Line *l = btor2parser_get_line_by_id (model, i);
      if (!l) continue;
      // these are not signals
      if (l->tag == BTOR2_TAG_sort || l->tag == BTOR2_TAG_init
          || l->tag == BTOR2_TAG_next || l->tag == BTOR2_TAG_bad
          || l->tag == BTOR2_TAG_constraint || l->tag == BTOR2_TAG_fair
          || l->tag == BTOR2_TAG_justice)
        continue;
      // only add named signals
      if (!l->symbol) continue;
      vcd_writer->add_value_change (k, i, current_state[i]);
    }
  }
}

static void
transition (int64_t k)
{
  msg (1, "transition %" PRId64, k);
  for (int64_t i = 0; i < num_format_lines; i++) delete_current_state (i);
  if (print_trace && print_states) printf ("#%" PRId64 "\n", k);
  for (size_t i = 0; i < states.size (); i++)
  {
    Btor2Line *state = states[i];
    assert (0 <= state->id), assert (state->id < num_format_lines);
    BtorSimState update = next_state[state->id];
    assert (update.is_set ());
    update_current_state (state->id, update);
    switch (next_state[state->id].type)
    {
      case BtorSimState::Type::BITVEC:
        next_state[state->id].bv_state = nullptr;
        break;
      case BtorSimState::Type::ARRAY:
        next_state[state->id].array_state = nullptr;
        break;
      default: die ("Invalid state type");
    }
    if (print_trace && print_states)
      print_state_or_input (state->id, i, k, false);
  }
}

static void
report ()
{
  if (verbosity && num_unreached_bads < (int64_t) bads.size ())
  {
    printf ("[btorsim] reached bad state properties {");
    for (size_t i = 0; i < bads.size (); i++)
    {
      int64_t r = reached_bads[i];
      if (r >= 0) printf (" b%lu@%" PRId64, i, r);
    }
    printf (" }\n");
  }
  else if (!bads.empty ())
    msg (1, "no bad state property reached");

  if (constraints_violated >= 0)
    msg (1, "constraints violated at time %" PRId64, constraints_violated);
  else if (!constraints.empty ())
    msg (1, "constraints always satisfied");
}

static void
random_simulation (int64_t k)
{
  msg (1, "starting random simulation up to bound %" PRId64, k);
  assert (k >= 0);

  const int32_t randomize = 1;

  initialize_states (randomize);
  initialize_inputs (0, randomize);
  simulate_step (0, randomize);

  for (int64_t i = 1; i <= k; i++)
  {
    if (constraints_violated >= 0) break;
    if (!num_unreached_bads) break;
    transition (i);
    initialize_inputs (i, randomize);
    simulate_step (i, randomize);
  }

  if (print_trace) printf (".\n"), fflush (stdout);
  report ();
}

static int64_t charno;
static int64_t columno;
static int64_t lineno = 1;
static int32_t saved_char;
static int32_t char_saved;
static uint64_t last_line_length;

static BtorCharStack array_index;
static BtorCharStack constant;
static BtorCharStack symbol;

static int32_t
next_char ()
{
  int32_t res;
  if (char_saved)
  {
    res        = saved_char;
    char_saved = 0;
  }
  else
  {
    res = getc (witness_file);
  }
  if (res == '\n')
  {
    last_line_length = columno;
    columno          = 0;
    lineno++;
  }
  else if (res != EOF)
  {
    columno++;
  }
  if (res != EOF) charno++;
  return res;
}

static void
prev_char (int32_t ch)
{
  assert (!char_saved);
  if (ch == '\n')
  {
    columno = last_line_length;
    assert (lineno > 0);
    lineno--;
  }
  else if (ch != EOF)
  {
    assert (charno > 0);
    charno--;
    assert (columno > 0);
    columno--;
  }
  saved_char = ch;
  char_saved = 1;
}

static void
parse_error (const char *msg, ...)
{
  fflush (stdout);
  assert (witness_path);
  fprintf (stderr,
           "*** 'btorsim' parse error in '%s' at line %" PRId64
           " column %" PRId64 ": ",
           witness_path,
           lineno,
           columno);
  va_list ap;
  va_start (ap, msg);
  vfprintf (stderr, msg, ap);
  va_end (ap);
  fprintf (stderr, "\n");
  exit (1);
}

static int64_t count_sat_witnesses;
static int64_t count_unsat_witnesses;
static int64_t count_unknown_witnesses;
static int64_t count_witnesses;

static std::vector<int64_t> claimed_bad_witnesses;
static std::vector<int64_t> claimed_justice_witnesses;

static int64_t
parse_unsigned_number (int32_t *ch_ptr)
{
  int32_t ch  = next_char ();
  int64_t res = 0;
  if (ch == '0')
  {
    ch = next_char ();
    if (isdigit (ch)) parse_error ("unexpected digit '%c' after '0'", ch);
  }
  else if (!isdigit (ch))
    parse_error ("expected digit");
  else
  {
    res = ch - '0';
    while (isdigit (ch = next_char ()))
    {
      if (LONG_MAX / 10 < res)
        parse_error ("number too large (too many digits)");
      res *= 10;
      const int32_t digit = ch - '0';
      if (LONG_MAX - digit < res) parse_error ("number too large");
      res += digit;
    }
  }
  *ch_ptr = ch;
  return res;
}

static int64_t constant_columno;
static int64_t index_columno;
static int32_t found_end_of_witness;
static int32_t found_initial_frame;

static int64_t
parse_assignment ()
{
  int32_t ch = next_char ();
  if (ch == EOF) parse_error ("unexpected end-of-file (without '.')");
  if (ch == '.')
  {
    while ((ch = next_char ()) == ' ')
      ;
    if (ch == EOF) parse_error ("end-of-file after '.' instead of new-line");
    if (ch != '\n')
    {
      if (isprint (ch))
        parse_error ("unexpected character '%c' after '.'", ch);
      else
        parse_error ("unexpected character code 0x%02x after '.'", ch);
    }
    msg (4, "read terminating '.'");
    found_end_of_witness = 1;
    return -1;
  }
  if (ch == '@' || ch == '#')
  {
    prev_char (ch);
    return -1;
  }
  prev_char (ch);
  int64_t res = parse_unsigned_number (&ch);
  if (ch != ' ') parse_error ("space missing after '%" PRId64 "'", res);
  ch = next_char ();
  BTOR2_RESET_STACK (array_index);
  if (ch == '[')
  {
    index_columno = columno + 1;
    while ((ch = next_char ()) == '0' || ch == '1')
      BTOR2_PUSH_STACK (array_index, ch);
    /* btormc trace compatibility: btormc --trace-gen-full represents
     * 'initialize all array elements' as <id> [*] <value> */
    if (ch == '*')
    {
      BTOR2_PUSH_STACK (array_index, ch);
      ch = next_char ();
    }
    if (ch != ']') parse_error ("expected ] after index");
    if (BTOR2_EMPTY_STACK (array_index)) parse_error ("empty index");
    BTOR2_PUSH_STACK (array_index, 0);
    ch = next_char ();
    if (ch != ' ') parse_error ("space missing after index");
  }
  else
  {
    prev_char (ch);
  }
  BTOR2_RESET_STACK (constant);
  constant_columno = columno + 1;
  while ((ch = next_char ()) == '0' || ch == '1')
    BTOR2_PUSH_STACK (constant, ch);
  if (BTOR2_EMPTY_STACK (constant))
    parse_error ("empty constant");
  else if (ch != ' ' && ch != '\n')
    parse_error ("expected space or new-line after assignment");
  BTOR2_PUSH_STACK (constant, 0);
  BTOR2_RESET_STACK (symbol);
  while (ch != '\n')
    if ((ch = next_char ()) == EOF)
      parse_error ("unexpected end-of-file in assignment");
    else if (ch != '\n')
      BTOR2_PUSH_STACK (symbol, ch);
  if (!BTOR2_EMPTY_STACK (symbol)) BTOR2_PUSH_STACK (symbol, 0);
  return res;
}

static void
parse_state_part (int64_t k)
{
  int32_t ch = next_char ();
  if (ch != '#')
  {
    prev_char (ch);
    return;
  }
  if (parse_unsigned_number (&ch) != k || ch != '\n')
    parse_error (
        "missing '#%" PRId64 "' state part header of frame %" PRId64, k, k);
  int64_t state_pos;
  while ((state_pos = parse_assignment ()) >= 0)
  {
    int64_t saved_charno = charno;
    charno               = 1;
    assert (lineno > 1);
    lineno--;
    if (state_pos >= (int64_t) states.size ())
      parse_error ("less than %" PRId64 " states defined", state_pos);
    Btor2Line *state = states[state_pos];
    assert (state);
    assert (0 <= state->id), assert (state->id < num_format_lines);
    bool is_const_init = false;
    if (state->sort.tag == BTOR2_TAG_SORT_bitvec)
    {
      if (BTOR2_EMPTY_STACK (symbol))
      {
        msg (4,
             "state assignment '%" PRId64 " %s' at time frame %" PRId64,
             state_pos,
             constant.start,
             k);
      }
      else
      {
        msg (4,
             "state assignment '%" PRId64 " %s %s' at time frame %" PRId64,
             state_pos,
             constant.start,
             symbol.start,
             k);
      }
      assert (BTOR2_EMPTY_STACK (array_index));
      if (strlen (constant.start) != state->sort.bitvec.width)
      {
        charno = constant_columno;
        parse_error ("expected constant of width '%u'",
                     state->sort.bitvec.width);
      }
    }
    else
    {
      if BTOR2_EMPTY_STACK (array_index)
      /* pono trace compatibility: pono represents 'initialize all array
       * elements' as <id> <value> (same syntax as for vector assignment, but
       * id is for a state of sort array) */
      {
        BTOR2_PUSH_STACK (array_index, '*');
        BTOR2_PUSH_STACK (array_index, 0);
      }
      if (BTOR2_EMPTY_STACK (symbol))
      {
        msg (4,
             "state assignment '%" PRId64 " [%s] %s' at time frame %" PRId64,
             state_pos,
             array_index.start,
             constant.start,
             k);
      }
      else
      {
        msg (4,
             "state assignment '%" PRId64 " [%s] %s %s' at time frame %" PRId64,
             state_pos,
             array_index.start,
             constant.start,
             symbol.start,
             k);
      }
      assert (state->sort.tag == BTOR2_TAG_SORT_array);
      Btor2Line *li =
          btor2parser_get_line_by_id (model, state->sort.array.index);
      Btor2Line *le =
          btor2parser_get_line_by_id (model, state->sort.array.element);
      assert (li->sort.tag == BTOR2_TAG_SORT_bitvec);
      assert (le->sort.tag == BTOR2_TAG_SORT_bitvec);
      is_const_init = BTOR2_COUNT_STACK (array_index) == 2
                      && BTOR2_PEEK_STACK (array_index, 0) == '*';
      if (strlen (array_index.start) != li->sort.bitvec.width && !is_const_init)
      {
        charno = index_columno;
        parse_error ("expected index of width '%u'", state->sort.array.index);
      }
      if (strlen (constant.start) != le->sort.bitvec.width)
      {
        charno = constant_columno;
        parse_error ("expected element of width '%u'",
                     state->sort.array.element);
      }
      if (!current_state[state->id].array_state)
      {
        current_state[state->id].array_state = new BtorSimArrayModel (
            li->sort.bitvec.width, le->sort.bitvec.width);
      }
      assert (current_state[state->id].array_state);
    }

    BtorSimBitVector *idx = 0;
    if (state->sort.tag == BTOR2_TAG_SORT_array && !is_const_init)
      idx = btorsim_bv_char_to_bv (array_index.start);
    BtorSimBitVector *val = btorsim_bv_char_to_bv (constant.start);
    if (k == 0)
    {
      Btor2Line *init = inits[state->id];
      if (init && nexts[state->id])
      {
        msg (4, "init & next for state %" PRId64, state->id);
        assert (init->nargs == 2);
        assert (init->args[0] == state->id);
        BtorSimState tmp = simulate (init->args[1]);
        if (state->sort.tag == BTOR2_TAG_SORT_bitvec)
        {
          assert (tmp.type == BtorSimState::Type::BITVEC);
          if (btorsim_bv_compare (val, tmp.bv_state))
            parse_error ("incompatible initialized state %" PRId64
                         " id %" PRId64,
                         state_pos,
                         state->id);
        }
        else
        {
          BtorSimBitVector *element;
          if (is_const_init)
          {
            assert (tmp.type == BtorSimState::Type::BITVEC);
            element = btorsim_bv_copy (tmp.bv_state);
          }
          else
          {
            assert (tmp.type == BtorSimState::Type::ARRAY);
            element = tmp.array_state->check (idx);
          }
          if (element)
          {
            if (btorsim_bv_compare (val, element))
            {
              parse_error ("incompatible initialized state %" PRId64
                           " id %" PRId64,
                           state_pos,
                           state->id);
            }
            btorsim_bv_free (element);
          }
        }
        tmp.remove ();
      }
    }
    lineno++;
    charno = saved_charno;
    if (k > 0 && nexts[state->id])
    {
      if (state->sort.tag == BTOR2_TAG_SORT_bitvec)
      {
        if (btorsim_bv_compare (val, current_state[state->id].bv_state))
        {
          parse_error ("incompatible assignment for state %" PRId64
                       " id %" PRId64 " in time frame %" PRId64,
                       state_pos,
                       state->id,
                       k);
        }
      }
      else
      {
        BtorSimBitVector *tmp =
            is_const_init
                ? current_state[state->id].array_state->get_const_init ()
                : current_state[state->id].array_state->check (idx);
        if (tmp && btorsim_bv_compare (val, tmp))
        {
          parse_error ("incompatible assignment for state %" PRId64
                       " id %" PRId64 " in time frame %" PRId64,
                       state_pos,
                       state->id,
                       k);
        }
        if (tmp) btorsim_bv_free (tmp);
      }
    }
    if (state->sort.tag == BTOR2_TAG_SORT_bitvec)
    {
      update_current_state (state->id, val);
    }
    else
    {
      assert (current_state[state->id].type == BtorSimState::Type::ARRAY);
      BtorSimState tmp;
      tmp.type = BtorSimState::Type::ARRAY;
      if (is_const_init)
      {
        tmp.array_state =
            current_state[state->id].array_state->set_const_init (val);
      }
      else
      {
        tmp.array_state =
            current_state[state->id].array_state->write (idx, val);
        btorsim_bv_free (idx);
      }
      update_current_state (state->id, tmp);
      btorsim_bv_free (val);
    }
  }
  if (!k) found_initial_frame = 1;
}

static void
parse_input_part (int64_t k)
{
  int32_t ch = next_char ();
  if (ch != '@' || parse_unsigned_number (&ch) != k || ch != '\n')
    parse_assignment ();
  int64_t input_pos;
  while ((input_pos = parse_assignment ()) >= 0)
  {
    int64_t saved_charno = charno;
    charno               = 1;
    assert (lineno > 1);
    lineno--;
    if (input_pos >= (int64_t) inputs.size ())
    {
      parse_error ("less than %" PRId64 " defined", input_pos);
    }
    if (BTOR2_EMPTY_STACK (array_index))
    {
      if (BTOR2_EMPTY_STACK (symbol))
      {
        msg (4,
             "input assignment '%" PRId64 " %s' at time frame %" PRId64,
             input_pos,
             constant.start,
             k);
      }
      else
      {
        msg (4,
             "input assignment '%" PRId64 " %s %s' at time frame %" PRId64,
             input_pos,
             constant.start,
             symbol.start,
             k);
      }
    }
    else
    {
      if (BTOR2_EMPTY_STACK (symbol))
      {
        msg (4,
             "input assignment '%" PRId64 " [%s] %s' at time frame %" PRId64,
             input_pos,
             array_index.start,
             constant.start,
             k);
      }
      else
      {
        msg (4,
             "input assignment '%" PRId64 " [%s] %s %s' at time frame %" PRId64,
             input_pos,
             array_index.start,
             constant.start,
             symbol.start,
             k);
      }
    }
    Btor2Line *input = inputs[input_pos];
    assert (input);
    if (current_state[input->id].type == BtorSimState::Type::BITVEC)
    {
      if (strlen (constant.start) != input->sort.bitvec.width)
      {
        charno = constant_columno;
        parse_error ("expected constant of width '%u'",
                     input->sort.bitvec.width);
      }
      assert (0 <= input->id), assert (input->id < num_format_lines);
      if (current_state[input->id].bv_state)
      {
        parse_error ("input %" PRId64 " id %" PRId64
                     " assigned twice in frame %" PRId64,
                     input_pos,
                     input->id,
                     k);
      }
      BtorSimBitVector *val = btorsim_bv_char_to_bv (constant.start);
      lineno++;
      charno = saved_charno;
      update_current_state (input->id, val);
    }
    else
    {
      assert (current_state[input->id].type == BtorSimState::Type::ARRAY);
      BtorSimBitVector *idx = btorsim_bv_char_to_bv (array_index.start);
      BtorSimBitVector *val = btorsim_bv_char_to_bv (constant.start);
      lineno++;
      charno = saved_charno;
      BtorSimArrayModel *am =
          current_state[input->id].array_state->write (idx, val);
      update_current_state (input->id, am);
    }
  }
}

static int32_t
parse_frame (int64_t k)
{
  if (k > 0) transition (k);
  msg (2, "parsing frame %" PRId64, k);
  parse_state_part (k);
  parse_input_part (k);
  const int32_t randomize = 0;
  if (!k) initialize_states (randomize);
  initialize_inputs (k, randomize);
  simulate_step (k, randomize);
  return !found_end_of_witness;
}

static void
parse_sat_witness ()
{
  assert (count_witnesses == 1);

  msg (1, "parsing 'sat' witness %" PRId64, count_sat_witnesses);

  for (;;)
  {
    int32_t type = next_char ();
    if (type == ' ') continue;
    if (type == '\n') break;
    ;
    if (type != 'b' && type != 'j') parse_error ("expected 'b' or 'j'");
    int32_t ch;
    int64_t bad = parse_unsigned_number (&ch);
    if (ch != ' ' && ch != '\n')
    {
      if (isprint (ch))
        parse_error (
            "unexpected '%c' after number (expected space or new-line)", ch);
      else
        parse_error (
            "unexpected character 0x%02x after number"
            " (expected space or new-line)",
            ch);
    }
    if (type == 'b')
    {
      if (bad >= (int64_t) bads.size ())
      {
        parse_error ("invalid bad state property number %" PRId64, bad);
      }
      msg (3,
           "... claims to be witness of bad state property number 'b%" PRId64
           "'",
           bad);
      claimed_bad_witnesses.push_back (bad);
    }
    else
    {
      parse_error ("can not handle justice properties yet");
    }
    if (ch == '\n') break;
  }

  int64_t k = 0;
  while (parse_frame (k)) k++;

  msg (1, "finished parsing k = %" PRId64 " frames", k);
  if (dump_vcd) vcd_writer->update_time (k + 1);

  report ();
  if (print_trace) printf (".\n"), fflush (stdout);

  for (size_t i = 0; i < claimed_bad_witnesses.size (); i++)
  {
    int64_t bad_pos = claimed_bad_witnesses[i];
    int64_t bound   = reached_bads[bad_pos];
    Btor2Line *l    = bads[bad_pos];
    if (bound < 0)
    {
      die ("claimed bad state property 'b%" PRId64 "' id %" PRId64
           " not reached",
           bad_pos,
           l->id);
    }
  }
}

static void
parse_unknown_witness ()
{
  msg (1, "parsing unknown witness %" PRId64, count_unknown_witnesses);
  int64_t k = 0;

  while (parse_frame (k)) k++;

  report ();
  if (print_trace) printf (".\n"), fflush (stdout);

  msg (1, "finished parsing k = %" PRId64 " frames", k);
}

static void
parse_unsat_witness ()
{
  msg (1, "parsing 'unsat' witness %" PRId64, count_unsat_witnesses);
  die ("'unsat' witnesses not supported yet");
}

static bool
parse_and_check_witness ()
{
  int32_t ch = next_char ();
  if (ch == EOF) return false;

  found_end_of_witness = 0;
  found_initial_frame  = 0;

  if (ch == '#')
  {
    count_witnesses++;
    count_unknown_witnesses++;
    if (count_sat_witnesses + count_unknown_witnesses > 1)
      die ("more than one actual witness not supported yet");
    prev_char (ch);
    parse_unknown_witness ();
    return true;
  }

  if (ch == 's')
  {
    if ((ch = next_char ()) == 'a' && (ch = next_char ()) == 't'
        && (ch = next_char ()) == '\n')
    {
      count_witnesses++;
      count_sat_witnesses++;
      msg (1,
           "found witness %" PRId64 " header 'sat' in '%s' at line %" PRId64,
           count_sat_witnesses,
           witness_path,
           lineno - 1);
      if (count_witnesses > 1)
        die ("more than one actual witness not supported yet");
      parse_sat_witness ();
      return true;
    }
  }

  if (ch == 'u')
  {
    if ((ch = next_char ()) == 'n' && (ch = next_char ()) == 's'
        && (ch = next_char ()) == 'a' && (ch = next_char ()) == 't'
        && (ch = next_char ()) == '\n')
    {
      count_witnesses++;
      count_unsat_witnesses++;
      msg (1,
           "found witness %" PRId64 " header 'unsat' in '%s' at line %" PRId64,
           witness_path,
           count_unsat_witnesses,
           lineno - 1);
      parse_unsat_witness ();
      return true;
    }
  }

  while (ch != '\n')
  {
    ch = next_char ();
    if (ch == EOF) parse_error ("unexpected end-of-file before new-line");
  }

  return true;
}

static void
parse_and_check_all_witnesses ()
{
  BTOR2_INIT_STACK (constant);
  BTOR2_INIT_STACK (symbol);
  BTOR2_INIT_STACK (array_index);
  assert (witness_file);
  while (parse_and_check_witness ())
    ;
  BTOR2_RELEASE_STACK (constant);
  BTOR2_RELEASE_STACK (symbol);
  BTOR2_RELEASE_STACK (array_index);
  msg (1,
       "finished parsing %" PRId64 " witnesses after reading %" PRId64
       " bytes (%.1f MB)",
       count_witnesses,
       charno,
       charno / (double) (1l << 20));
}

void
setup_states ()
{
  current_state.resize (num_format_lines);
  next_state.resize (num_format_lines);
  if (dump_vcd) vcd_writer->prev_value.resize (num_format_lines);

  for (int i = 0; i < num_format_lines; i++)
  {
    Btor2Line *l = btor2parser_get_line_by_id (model, i);
    if (l)
    {
      Btor2Sort *sort = get_sort (l, model);
      switch (sort->tag)
      {
        case BTOR2_TAG_SORT_bitvec:
          current_state[i].type = BtorSimState::Type::BITVEC;
          next_state[i].type    = BtorSimState::Type::BITVEC;
          if (dump_vcd)
          {
            vcd_writer->prev_value[i].type = BtorSimState::Type::BITVEC;
          }
          break;
        case BTOR2_TAG_SORT_array:
          current_state[i].type = BtorSimState::Type::ARRAY;
          next_state[i].type    = BtorSimState::Type::ARRAY;
          if (dump_vcd)
          {
            vcd_writer->prev_value[i].type = BtorSimState::Type::ARRAY;
          }
          break;
        default: die ("Unknown sort");
      }
    }
  }

  for (auto state : states)
  {
    assert (current_state[state->id].type != BtorSimState::Type::INVALID);
    assert (next_state[state->id].type != BtorSimState::Type::INVALID);
  }
}

int32_t
main (int32_t argc, char const *argv[])
{
  const char *info_path = nullptr;
  int64_t fake_bad = -1, fake_justice = -1;
  int32_t r = -1, s = -1;
  for (int32_t i = 1; i < argc; i++)
  {
    if (!strcmp (argv[i], "-h"))
      fputs (usage, stdout), exit (0);
    else if (!strcmp (argv[i], "-c"))
      print_trace = false;
    else if (!strcmp (argv[i], "-v"))
      verbosity++;
    else if (!strcmp (argv[i], "-r"))
    {
      if (++i == argc) die ("argument to '-r' missing");
      if (!parse_int (argv[i], &r)) die ("invalid number in '-r %s'", argv[i]);
    }
    else if (!strcmp (argv[i], "-s"))
    {
      if (++i == argc) die ("argument to '-s' missing");
      if (!parse_int (argv[i], &s)) die ("invalid number in '-s %s'", argv[i]);
    }
    else if (!strcmp (argv[i], "-b"))
    {
      if (++i == argc) die ("argument to '-b' missing");
      if (!parse_long (argv[i], &fake_bad))
        die ("invalid number in '-b %s'", argv[i]);
    }
    else if (!strcmp (argv[i], "-j"))
    {
      if (++i == argc) die ("argument to '-j' missing");
      if (!parse_long (argv[i], &fake_justice))
        die ("invalid number in '-j %s'", argv[i]);
    }
    else if (!strcmp (argv[i], "--states"))
      print_states = true;
    else if (!strcmp (argv[i], "--vcd"))
    {
      dump_vcd = true;
      if (++i == argc) die ("argument to '--vcd' missing");
      vcd_path = argv[i];
    }
    else if (!strcmp (argv[i], "--hierarchical-symbols"))
      symbol_fmt = true;
    else if (!strcmp (argv[i], "--info"))
    {
      if (++i == argc) die ("argument to '--info' missing");
      info_path = argv[i];
    }
    else if (argv[i][0] == '-')
      die ("invalid command line option '%s' (try '-h')", argv[i]);
    else if (witness_path)
      die ("too many file arguments '%s', '%s', and '%s'",
           model_path,
           witness_path,
           argv[i]);
    else if (model_path)
      witness_path = argv[i];
    else
      model_path = argv[i];
  }
  if (model_path)
  {
    if (!(model_file = fopen (model_path, "r")))
      die ("failed to open BTOR model file '%s' for reading", model_path);
    close_model_file = 1;
  }
  else
  {
    model_path = "<stdin>";
    model_file = stdin;
  }
  if (witness_path)
  {
    if (!(witness_file = fopen (witness_path, "r")))
      die ("failed to open witness file '%s' for reading", witness_path);
    close_witness_file = 1;
  }
  if (model_path && witness_path)
  {
    msg (1, "checking mode: both model and witness specified");
    checking_mode = 1;
    random_mode   = 0;
  }
  else
  {
    msg (1, "random mode: witness not specified");
    checking_mode = 0;
    random_mode   = 1;
  }
  if (checking_mode)
  {
    if (r >= 0)
      die ("number of random test vectors specified in checking mode");
    if (s >= 0) die ("random seed specified in checking mode");
    if (fake_bad >= 0) die ("can not fake bad state property in checking mode");
    if (fake_justice >= 0)
      die ("can not fake justice property in checking mode");
  }
  if (dump_vcd)
  {
    vcd_writer = new BtorSimVCDWriter (vcd_path, readable_vcd, symbol_fmt);
    if (info_path)
    {
      extra_constraints = vcd_writer->read_info_file (info_path);
    }
  }
  assert (model_path);
  msg (1, "reading BTOR model from '%s'", model_path);
  parse_model ();
  if (fake_bad >= (int64_t) bads.size ())
    die ("invalid faked bad state property number %" PRId64, fake_bad);
  if (fake_justice >= (int64_t) justices.size ())
    die ("invalid faked justice property number %" PRId64, fake_justice);
  if (close_model_file && fclose (model_file))
    die ("can not close model file '%s'", model_path);
  setup_states ();
  if (random_mode)
  {
    if (r < 0) r = 20;
    if (s < 0) s = 0;
    msg (1, "using random seed %d", s);
    btorsim_rng_init (&rng, (uint32_t) s);
    if (print_trace)
    {
      if (fake_bad >= 0 && fake_justice >= 0)
        printf ("sat\nb%" PRId64 " j%" PRId64 "\n", fake_bad, fake_justice);
      else if (fake_bad >= 0)
        printf ("sat\nb%" PRId64 "\n", fake_bad);
      else if (fake_justice >= 0)
        printf ("sat\nj%" PRId64 "\n", fake_justice);
    }
    random_simulation (r);
  }
  else
  {
    assert (witness_path);
    msg (1, "reading BTOR witness from '%s'", witness_path);
    parse_and_check_all_witnesses ();
    if (close_witness_file && fclose (witness_file))
      die ("can not close witness file '%s'", witness_path);
  }
  if (dump_vcd)
  {
    vcd_writer->write_vcd (model);
    delete vcd_writer;
  }
  btor2parser_delete (model);
  for (int64_t i = 0; i < num_format_lines; i++)
    if (current_state[i].type) current_state[i].remove ();
  for (int64_t i = 0; i < num_format_lines; i++)
    if (next_state[i].type) next_state[i].remove ();
  return 0;
}
