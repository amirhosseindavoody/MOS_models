
// Netlist parser implementation
//
// Parses SPICE-style netlists into Circuit structures.
//

#include "parser.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace minispice {

// ============================================================================
// Utility Functions
// ============================================================================

static std::string trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

static std::string to_upper(const std::string& s) {
  std::string result = s;
  for (char& c : result) {
    c = std::toupper(static_cast<unsigned char>(c));
  }
  return result;
}

// Parse value with optional suffix (k, M, G, m, u, n, p, f)
static double parse_value(const std::string& str) {
  if (str.empty()) return 0.0;

  size_t idx = 0;
  double value = std::stod(str, &idx);

  if (idx < str.size()) {
    char suffix = std::tolower(static_cast<unsigned char>(str[idx]));
    switch (suffix) {
      case 't':
        value *= 1e12;
        break;
      case 'g':
        value *= 1e9;
        break;
      case 'x':  // MEG
      case 'm':
        if (idx + 2 < str.size() && std::tolower(str[idx + 1]) == 'e' &&
            std::tolower(str[idx + 2]) == 'g') {
          value *= 1e6;
        } else if (idx + 2 < str.size() && std::tolower(str[idx + 1]) == 'i' &&
                   std::tolower(str[idx + 2]) == 'l') {
          value *= 25.4e-6;  // mil
        } else {
          value *= 1e-3;  // milli
        }
        break;
      case 'k':
        value *= 1e3;
        break;
      case 'u':
        value *= 1e-6;
        break;
      case 'n':
        value *= 1e-9;
        break;
      case 'p':
        value *= 1e-12;
        break;
      case 'f':
        value *= 1e-15;
        break;
      default:
        break;
    }
  }

  return value;
}

// Parse key=value parameter
static bool parse_param(const std::string& token, const std::string& key,
                        double* value) {
  std::string upper_token = to_upper(token);
  std::string upper_key = to_upper(key) + "=";

  if (upper_token.find(upper_key) == 0) {
    std::string val_str = token.substr(upper_key.length());
    *value = parse_value(val_str);
    return true;
  }
  return false;
}

// ============================================================================
// Parser Implementation
// ============================================================================

static Circuit* parse_lines(const std::vector<std::string>& lines) {
  Circuit* c = circuit_create();
  if (!c) return nullptr;

  for (const auto& raw_line : lines) {
    std::string line = trim(raw_line);

    // Skip empty lines and comments
    if (line.empty()) continue;
    if (line[0] == '*' || line[0] == '#') continue;
    if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;
    if (line[0] == '.') continue;  // Skip directives for now

    // Tokenize
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
      tokens.push_back(token);
    }

    if (tokens.empty()) continue;

    std::string name = tokens[0];
    char type = std::toupper(static_cast<unsigned char>(name[0]));

    if (type == 'R') {
      // Resistor: Rname n1 n2 value
      if (tokens.size() < 4) {
        fprintf(stderr, "Parser error: Invalid resistor line: %s\n",
                line.c_str());
        continue;
      }
      int n1 = circuit_add_node(c, tokens[1].c_str());
      int n2 = circuit_add_node(c, tokens[2].c_str());
      double value = parse_value(tokens[3]);

      int v1 = circuit_get_var_index(c, n1);
      int v2 = circuit_get_var_index(c, n2);
      // For now, store node indices; var indices assigned after finalize
      // We'll use a different approach - store node indices and convert later

      Device* d = create_resistor(name.c_str(), n1, n2, value);
      if (d) circuit_add_device(c, d);

    } else if (type == 'I') {
      // Current Source: Iname n1 n2 value
      if (tokens.size() < 4) {
        fprintf(stderr, "Parser error: Invalid current source line: %s\n",
                line.c_str());
        continue;
      }
      int n1 = circuit_add_node(c, tokens[1].c_str());
      int n2 = circuit_add_node(c, tokens[2].c_str());
      double value = parse_value(tokens[3]);

      Device* d = create_current_source(name.c_str(), n1, n2, value);
      if (d) circuit_add_device(c, d);

    } else if (type == 'V') {
      // Voltage Source: Vname n1 n2 value
      if (tokens.size() < 4) {
        fprintf(stderr, "Parser error: Invalid voltage source line: %s\n",
                line.c_str());
        continue;
      }
      int n1 = circuit_add_node(c, tokens[1].c_str());
      int n2 = circuit_add_node(c, tokens[2].c_str());
      double value = parse_value(tokens[3]);

      Device* d = create_voltage_source(name.c_str(), n1, n2, value);
      if (d) circuit_add_device(c, d);

    } else if (type == 'C') {
      // Capacitor: Cname n1 n2 value
      if (tokens.size() < 4) {
        fprintf(stderr, "Parser error: Invalid capacitor line: %s\n",
                line.c_str());
        continue;
      }
      int n1 = circuit_add_node(c, tokens[1].c_str());
      int n2 = circuit_add_node(c, tokens[2].c_str());
      double value = parse_value(tokens[3]);

      Device* d = create_capacitor(name.c_str(), n1, n2, value);
      if (d) circuit_add_device(c, d);

    } else if (type == 'L') {
      // Inductor: Lname n1 n2 value
      if (tokens.size() < 4) {
        fprintf(stderr, "Parser error: Invalid inductor line: %s\n",
                line.c_str());
        continue;
      }
      int n1 = circuit_add_node(c, tokens[1].c_str());
      int n2 = circuit_add_node(c, tokens[2].c_str());
      double value = parse_value(tokens[3]);

      Device* d = create_inductor(name.c_str(), n1, n2, value);
      if (d) circuit_add_device(c, d);

    } else if (type == 'D') {
      // Diode: Dname anode cathode [Is=value] [n=value]
      if (tokens.size() < 3) {
        fprintf(stderr, "Parser error: Invalid diode line: %s\n", line.c_str());
        continue;
      }
      int n_anode = circuit_add_node(c, tokens[1].c_str());
      int n_cathode = circuit_add_node(c, tokens[2].c_str());

      // Default diode parameters
      double I_s = 1e-14;
      double n = 1.0;

      // Parse optional parameters
      for (size_t i = 3; i < tokens.size(); i++) {
        double val;
        if (parse_param(tokens[i], "Is", &val)) {
          I_s = val;
        } else if (parse_param(tokens[i], "n", &val)) {
          n = val;
        }
      }

      Device* d = create_diode(name.c_str(), n_anode, n_cathode, I_s, n);
      if (d) circuit_add_device(c, d);

    } else {
      // Unknown element type - skip
      fprintf(stderr, "Parser warning: Unknown element type: %s\n",
              name.c_str());
    }
  }

  // After parsing, we need to update device node indices to var indices
  // This happens during circuit_finalize, but devices store node indices
  // We'll fix up device nodes after finalization

  return c;
}

// ============================================================================
// Public API
// ============================================================================

extern "C" {

Circuit* parse_netlist_file(const char* filepath) {
  if (!filepath) return nullptr;

  std::ifstream file(filepath);
  if (!file) {
    fprintf(stderr, "Parser error: Cannot open file: %s\n", filepath);
    return nullptr;
  }

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(file, line)) {
    lines.push_back(line);
  }

  Circuit* c = parse_lines(lines);
  if (!c) return nullptr;

  // Finalize: assigns var indices and initializes devices
  // But first, we need to fix up device node indices to var indices
  if (circuit_finalize(c) != 0) {
    circuit_free(c);
    return nullptr;
  }

  // Now update device node arrays from node indices to var indices
  for (Device* d = c->devices; d; d = d->next) {
    for (int i = 0; i < 4; i++) {
      int node_idx = d->nodes[i];
      if (node_idx >= 0 && node_idx < c->num_nodes) {
        d->nodes[i] = c->nodes[node_idx].var_index;
      }
    }
  }

  return c;
}

Circuit* parse_netlist_string(const char* netlist) {
  if (!netlist) return nullptr;

  std::vector<std::string> lines;
  std::istringstream iss(netlist);
  std::string line;
  while (std::getline(iss, line)) {
    lines.push_back(line);
  }

  Circuit* c = parse_lines(lines);
  if (!c) return nullptr;

  if (circuit_finalize(c) != 0) {
    circuit_free(c);
    return nullptr;
  }

  // Update device node arrays from node indices to var indices
  for (Device* d = c->devices; d; d = d->next) {
    for (int i = 0; i < 4; i++) {
      int node_idx = d->nodes[i];
      if (node_idx >= 0 && node_idx < c->num_nodes) {
        d->nodes[i] = c->nodes[node_idx].var_index;
      }
    }
  }

  return c;
}

}  // extern "C"

}  // namespace minispice