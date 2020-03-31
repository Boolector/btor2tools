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

#ifndef BTOR2VCD_H_INCLUDED
#define BTOR2VCD_H_INCLUDED

#include <vector>
#include <map>
#include <fstream>
#include <string>
#include <cinttypes>

#include "btor2parser/btor2parser.h"
#include "btorsimstate.h"
#include "btorsimhelpers.h"

static const char id_start = 33;
static const char id_end = 127;

class ModuleTreeNode
{
public:
  std::string name;
  std::map<int64_t, std::pair<std::string, uint32_t>> wire_names;
  std::vector<ModuleTreeNode*> submodules;
  ModuleTreeNode (std::string name) : name(name) {};
  ModuleTreeNode (const char* s) : name(std::string(s)) {};
  ~ModuleTreeNode ();
  void sort_name (int64_t id, std::string symbol, uint32_t width, bool symbol_fmt);
};

class BtorSimVCDWriter {
private:
  const bool readable_vcd;
  const bool symbol_fmt;
  std::ofstream vcd_file;
  int current_id;
  int64_t current_step;
  std::map<int64_t, std::string> bv_identifiers;
  std::map<std::pair<int64_t, std::string>, std::string> am_identifiers;
  std::vector<std::string> value_changes;
  std::string topname;
  enum ClkType {POSEDGE, NEGEDGE, EVENT};
  std::map<int64_t, ClkType> clocks;
public:
  std::vector<BtorSimState> prev_value;
  void write_vcd (Btor2Parser *model);
  std::string get_am_identifier (int64_t id, std::string);
  std::string get_bv_identifier (int64_t id);
  std::string generate_next_identifier ();
  void setup (Btor2Parser *model, int num_format_lines);
  void update_time (int64_t k);
  void add_value_change (int64_t k, int64_t id, BtorSimState state);
  ModuleTreeNode* sort_names (Btor2Parser *model, std::string topname);
  void write_node_header (ModuleTreeNode* top);
  std::map<int64_t, std::string> read_info_file(const char* info_path);
  BtorSimVCDWriter (const char* vcd_path, bool readable_vcd, bool symbol_fmt);
  ~BtorSimVCDWriter ();
};

#endif
