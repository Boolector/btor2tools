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

#include <cinttypes>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "btor2parser/btor2parser.h"
#include "btorsimhelpers.h"
#include "btorsimstate.h"

/* permissible character range for identifiers */
static const char id_start = 33;
static const char id_end   = 127;

/* helper class for building module hierarchy if symbol_fmt is set */
class ModuleTreeNode
{
 public:
  std::string name;
  std::map<int64_t, std::pair<std::string, uint32_t>> wire_names;
  std::vector<ModuleTreeNode*> submodules;
  ModuleTreeNode (std::string name) : name (name){};
  ModuleTreeNode (const char* s) : name (std::string (s)){};
  ~ModuleTreeNode ();
  void sort_name (int64_t id,
                  std::string symbol,
                  uint32_t width,
                  bool symbol_fmt);
};

/* VCD file handling */
class BtorSimVCDWriter
{
 private:
  const bool readable_vcd;  // use readable identifiers (enabled in debug mode)
  const bool symbol_fmt;    // interpret '.' in symbols as module hierarchy
  std::ofstream vcd_file;   // output VCD file
  int current_id;           // last assigned VCD identifier
  int64_t current_step;     // last seen time step
  std::map<int64_t, std::string>
      bv_identifiers;  // identifiers assigned to vector states
  std::map<std::pair<int64_t, std::string>, std::string>
      am_identifiers;  // identifiers assigned to array state elements
  std::vector<std::string>
      value_changes;  // contents of VCD file, one entry per line (in memory as
                      // header can only be written at end of simulation)

  /* optional information that can be given in info file */
  std::string topname;  // name of topmodule (default "top")
  enum ClkType
  {
    POSEDGE,
    NEGEDGE,
    EVENT
  };
  std::map<int64_t, ClkType> clocks;  // signals for which clock behavior of the
                                      // given type should be added

  /* retrieve the identifier for an array element (creates new if not previously
   * seen) */
  std::string get_am_identifier (int64_t id, std::string);

  /* retrieve the identifier for a vector state (creates new if not previously
   * seen) */
  std::string get_bv_identifier (int64_t id);

  /* create a new identifier (used by get_*_identifier) */
  std::string generate_next_identifier ();

  ModuleTreeNode* sort_names (Btor2Parser* model, std::string topname);  //
  void write_node_header (ModuleTreeNode* top);

 public:
  /* last seen value of states, to determine if it was changed in current time
   * step (public so it can be initialized in setup_states) */
  std::vector<BtorSimState> prev_value;

  BtorSimVCDWriter (const char* vcd_path, bool readable_vcd, bool symbol_fmt);
  ~BtorSimVCDWriter ();

  /* Read optional info file for additional information to include in VCD.
   * The format is whitespace separated key-value, one line per pair. Key values
   are:
     - name <string>: sets the name of the top module in the VCD.
     - (posedge|negedge|event) <int>: treats the state with the given ID as a
   clock of the given polarity, in the first two cases introducing value changes
   inbetween simulation steps, and for event changing the type of VCD signal.
     - bad <int>: treat the state with the given ID as a bad state and check
   that it stays false during simulation (for sanity-checking cover traces).
     Returns map of additional bad ID to associated symbol.
   */
  std::map<int64_t, std::string> read_info_file (const char* info_path);

  /* Move to next time step. Done automatically by add_value_change but needs to
   * be called once more at end to make last step visible */
  void update_time (int64_t k);

  /* Check if value of state changed at step k, and if yes add to VCD. To be
   * called on all states every time step. */
  void add_value_change (int64_t k, int64_t id, BtorSimState state);

  /* call at end of simulation to write the vcd_file */
  void write_vcd (Btor2Parser* model);
};

#endif
