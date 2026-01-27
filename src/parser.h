// parser.h
// Netlist parser for SPICE-like circuit description files
// Parses simple SPICE-style netlists and builds a Circuit structure.

#ifndef PARSER_H
#define PARSER_H

#include "circuit.h"

namespace minispice {

#ifdef __cplusplus
extern "C" {
#endif

// Parse a netlist file and create a Circuit
// Supported elements:
//   Resistor:       Rname n1 n2 value
//   Current Source: Iname n1 n2 value
//   Voltage Source: Vname n1 n2 value
//   Capacitor:      Cname n1 n2 value
//   Inductor:       Lname n1 n2 value
//   Diode:          Dname anode cathode [Is=value] [n=value]
// Comments start with * or # or //
// @param filepath Path to netlist file
// @return Pointer to new Circuit, or NULL on error
Circuit* parse_netlist_file(const char* filepath);

// Parse a netlist from a string
// Same format as parse_netlist_file.
// Returns Pointer to new Circuit, or NULL on error
Circuit* parse_netlist_string(const char* netlist);

#ifdef __cplusplus
}
#endif

}  // namespace minispice

#endif /* PARSER_H */
