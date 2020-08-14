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

#include "btorsimvcd.h"

#include <cassert>
#include <fstream>
#include <sstream>

BtorSimVCDWriter::BtorSimVCDWriter (const char* vcd_path,
                                    bool readable_vcd,
                                    bool symbol_fmt)
    : readable_vcd (readable_vcd), symbol_fmt (symbol_fmt)
{
  topname      = "top";
  current_id   = 0;
  current_step = -1;
  vcd_file.open (vcd_path);
}

BtorSimVCDWriter::~BtorSimVCDWriter ()
{
  vcd_file.close ();
  for (std::vector<BtorSimState>::size_type i = 0; i < prev_value.size (); i++)
    if (prev_value[i].type != BtorSimState::Type::INVALID)
      prev_value[i].remove ();
}

std::string
BtorSimVCDWriter::generate_next_identifier ()
{
  int rid = current_id++;
  std::string ret;
  do
  {
    char rem = rid % (id_end - id_start);
    ret += (id_start + rem);
    rid = rid / (id_end - id_start);
  } while (rid > 0);
  return ret;
}

std::string
BtorSimVCDWriter::get_bv_identifier (int64_t id)
{
  if (bv_identifiers.find (id) == bv_identifiers.end ())
  {
    if (readable_vcd)
      bv_identifiers[id] = "n" + std::to_string (id);
    else
      bv_identifiers[id] = generate_next_identifier ();
  }
  return bv_identifiers[id];
}

std::string
BtorSimVCDWriter::get_am_identifier (int64_t id, std::string idx)
{
  BtorSimBitVector* bv_idx = btorsim_bv_char_to_bv (idx.c_str ());
  auto key = std::make_pair (id, btorsim_bv_to_hex_string (bv_idx));
  btorsim_bv_free (bv_idx);
  if (am_identifiers.find (key) == am_identifiers.end ())
  {
    if (readable_vcd)
    {
      am_identifiers[key] = "n" + std::to_string (id) + "@" + key.second;
    }
    else
      am_identifiers[key] = generate_next_identifier ();
  }
  return am_identifiers[key];
}

ModuleTreeNode::~ModuleTreeNode ()
{
  for (ModuleTreeNode* m : submodules) delete m;
}

void
ModuleTreeNode::sort_name (int64_t id,
                           std::string symbol,
                           uint32_t width,
                           bool symbol_fmt)
{
  assert (symbol.length () > 0);
  size_t offset      = (symbol_fmt && symbol[0] == '\\') ? 1 : 0;
  size_t pos         = symbol_fmt ? symbol.find ('.') : std::string::npos;
  std::string s_name = symbol.substr (
      offset, pos - offset);  // npos is larger than all strings so npos-1 still
                              // means end of string no matter the length
  if (pos == std::string::npos)
  {
    wire_names[id] = std::make_pair (s_name, width);
    return;
  }

  for (ModuleTreeNode* m : submodules)
  {
    if (m->name == s_name)
    {
      m->sort_name (id, symbol.substr (pos + 1), width, symbol_fmt);
      return;
    }
  }
  ModuleTreeNode* m = new ModuleTreeNode (s_name);
  submodules.push_back (m);
  m->sort_name (id, symbol.substr (pos + 1), width, symbol_fmt);
}

ModuleTreeNode*
BtorSimVCDWriter::sort_names (Btor2Parser* model, std::string topname)
{
  ModuleTreeNode* top = new ModuleTreeNode (topname);
  for (auto i : bv_identifiers)
  {
    Btor2Line* l = btor2parser_get_line_by_id (model, i.first);
    assert (l);
    assert (l->symbol);
    Btor2Sort* sort = get_sort (l, model);
    assert (sort->tag == BTOR2_TAG_SORT_bitvec);
    top->sort_name (
        i.first, std::string (l->symbol), sort->bitvec.width, symbol_fmt);
  }
  for (auto i : am_identifiers)
  {
    int64_t id   = i.first.first;
    Btor2Line* l = btor2parser_get_line_by_id (model, id);
    assert (l);
    assert (l->symbol);
    Btor2Sort* sort = get_sort (l, model);
    assert (sort->tag == BTOR2_TAG_SORT_array);
    Btor2Line* le = btor2parser_get_line_by_id (model, sort->array.element);
    top->sort_name (
        id, std::string (l->symbol), le->sort.bitvec.width, symbol_fmt);
  }
  return top;
}

std::map<int64_t, std::string>
BtorSimVCDWriter::read_info_file (const char* info_path)
{
  std::map<int64_t, std::string> extra_bads;
  std::ifstream infofile (info_path);
  if (infofile.is_open ())
  {
    int lineno = 0;
    std::string line;
    while (getline (infofile, line))
    {
      lineno++;
      std::istringstream iss (line);
      std::string key;
      if (!(iss >> key)) continue;
      if (key == "name")
      {
        iss >> topname;
        msg (2, "Info file: found top module name: %s", topname.c_str ());
        continue;
      }
      if (key == "posedge")
      {
        int64_t clk_id;
        iss >> clk_id;
        clocks[clk_id] = POSEDGE;
        msg (2, "Info file: found posedge clock %d", clk_id);
        continue;
      }
      if (key == "negedge")
      {
        int64_t clk_id;
        iss >> clk_id;
        clocks[clk_id] = NEGEDGE;
        msg (2, "Info file: found negedge clock %d", clk_id);
        continue;
      }
      if (key == "event")
      {
        int64_t clk_id;
        iss >> clk_id;
        clocks[clk_id] = EVENT;
        msg (2, "Info file: found event clock %d", clk_id);
        continue;
      }
      if (key == "bad")
      {
        int64_t id;
        std::string symbol;
        iss >> id >> symbol;
        extra_bads[id] = symbol;
        msg (2, "Info file: found extra bad %d %s", id, symbol.c_str ());
        continue;
      }
      msg (
          1, "Failed to parse line %d in info file: %s", lineno, line.c_str ());
    }
    infofile.close ();
  }
  return extra_bads;
}

void
BtorSimVCDWriter::write_node_header (ModuleTreeNode* top)
{
  vcd_file << "$scope module " << top->name << " $end\n";
  for (auto i : top->wire_names)
  {
    int64_t id         = i.first;
    std::string symbol = i.second.first;
    uint32_t width     = i.second.second;
    if (bv_identifiers.find (id) != bv_identifiers.end ())
    {
      std::string type =
          (clocks.find (id) != clocks.end () && clocks[id] == EVENT) ? "event"
                                                                     : "wire";
      vcd_file << "$var " << type << " " << width << " "
               << bv_identifiers[i.first] << " " << symbol << " $end\n";
    }
    else
      for (auto j : am_identifiers)
      {
        if (j.first.first == id)
        {
          std::string idx      = j.first.second;
          std::string am_ident = j.second;
          vcd_file << "$var wire " << width << " " << am_ident << " " << symbol
                   << "<" << std::hex << idx << std::dec << "> $end\n";
        }
      }
  }
  for (ModuleTreeNode* s : top->submodules)
  {
    write_node_header (s);
  }
  vcd_file << "$upscope $end\n";
}

void
BtorSimVCDWriter::write_vcd (Btor2Parser* model)
{
  vcd_file << "$version\n\t Generated by btorsim\n$end\n";
  vcd_file << "$timescale 1ns $end\n";
  ModuleTreeNode* top = sort_names (model, topname);
  write_node_header (top);
  delete top;
  vcd_file << "$enddefinitions $end\n";

  for (std::string s : value_changes) vcd_file << s << "\n";
}

void
BtorSimVCDWriter::update_time (int64_t k)
{
  if (current_step < k)
  {
    if (k > 0)
    {
      value_changes.push_back ("#" + std::to_string (k * 10 - 5));
      for (auto clk : clocks)
      {
        switch (clk.second)
        {
          case POSEDGE:
            value_changes.push_back ("0" + get_bv_identifier (clk.first));
            break;
          case NEGEDGE:
            value_changes.push_back ("1" + get_bv_identifier (clk.first));
            break;
          case EVENT: break;
        }
      }
    }
    value_changes.push_back ("#" + std::to_string (k * 10));
    current_step = k;
    for (auto clk : clocks)
    {
      switch (clk.second)
      {
        case POSEDGE:
          value_changes.push_back ("1" + get_bv_identifier (clk.first));
          break;
        case NEGEDGE:
          value_changes.push_back ("0" + get_bv_identifier (clk.first));
          break;
        case EVENT:
          value_changes.push_back ("1" + get_bv_identifier (clk.first));
          break;
      }
    }
  }
}

void
BtorSimVCDWriter::add_value_change (int64_t k, int64_t id, BtorSimState state)
{
  if (clocks.find (id) != clocks.end ()) return;
  switch (state.type)
  {
    case BtorSimState::Type::BITVEC: {
      if (!state.bv_state)
      {
        msg (1, "No current state for named state %" PRId64 "!", id);
        return;
      }
      if (!prev_value[id].bv_state
          || btorsim_bv_compare (state.bv_state, prev_value[id].bv_state))
      {
        update_time (k);
        std::string sval ("");
        if (state.bv_state->width > 1) sval += "b";
        sval += btorsim_bv_to_string (state.bv_state);
        if (state.bv_state->width > 1) sval += " ";
        value_changes.push_back (sval + get_bv_identifier (id));
        prev_value[id].update (btorsim_bv_copy (state.bv_state));
      }
    }
    break;
    case BtorSimState::Type::ARRAY: {
      if (!state.array_state)
      {
        msg (1, "No current state for named state %" PRId64 "!", id);
        return;
      }

      if (!prev_value[id].array_state
          || state.array_state != prev_value[id].array_state)
      {
        update_time (k);
        for (auto it : state.array_state->data)
        {
          if (!prev_value[id].array_state
              || prev_value[id].array_state->data.find (it.first)
                     == prev_value[id].array_state->data.end ()
              || prev_value[id].array_state->data.at (it.first) != it.second)
          {
            std::string sval ("");
            if (it.second->width > 1) sval += "b";
            sval += btorsim_bv_to_string (it.second);
            if (it.second->width > 1) sval += " ";
            value_changes.push_back (sval + get_am_identifier (id, it.first));
          }
        }
        prev_value[id].update (state.array_state->copy ());
      }
    }
    break;
    default: die ("Invalid state");
  }
}
