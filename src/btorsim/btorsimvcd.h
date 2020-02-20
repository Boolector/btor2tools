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

class BtorSimVCDWriter {
private:
	const bool readable_vcd;
	const bool yosys_fmt;
	std::ofstream vcd_file;
	int current_id;
	int64_t current_step;
	std::map<int64_t, std::string> bv_identifiers;
	std::map<std::pair<int64_t, int64_t>, std::string> am_identifiers;
	std::vector<std::string> value_changes;
public:
	std::vector<BtorSimState> prev_value;
	void write_vcd (Btor2Parser *model);
	std::string get_am_identifier (int64_t id, int64_t idx);
	std::string get_bv_identifier (int64_t id);
	std::string generate_next_identifier ();
	void setup (Btor2Parser *model, int num_format_lines);
	void update_time (int64_t k);
	void add_value_change (int64_t k, int64_t id, BtorSimState state);
	BtorSimVCDWriter (const char* vcd_path, bool readable_vcd, bool yosys_fmt);
	~BtorSimVCDWriter ();
};

#endif
