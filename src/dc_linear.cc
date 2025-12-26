// Minimal DC MNA linear prototype for mini-spice
// Supports: Resistors (Rname n1 n2 value), Voltage sources (Vname n1 n2 value),
// Current sources (Iname n1 n2 value)

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using std::cerr;
using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;
using std::swap;
using std::unordered_map;
using std::vector;

// Circuit element representation
struct Element {
  char type;  // 'R', 'V', 'I'
  std::string name;
  std::string n1, n2;
  double value;
};

// trim whitespace
static std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

// parse netlist file
vector<Element> parse_netlist(const std::string& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("cannot open netlist");
  vector<Element> elems;
  std::string line;
  while (std::getline(f, line)) {
    line = trim(line);
    if (line.empty()) continue;
    if (line[0] == '*' || line[0] == '#' || line.rfind("//", 0) == 0) continue;
    // simple whitespace-separated tokens
    std::stringstream ss(line);
    std::string name;
    ss >> name;
    if (name.empty()) continue;
    char t = toupper(name[0]);
    if (t != 'R' && t != 'V' && t != 'I') continue;  // ignore unknown
    std::string n1, n2, valstr;
    ss >> n1 >> n2 >> valstr;
    if (n1.empty() || n2.empty() || valstr.empty()) continue;
    double val = std::stod(valstr);
    elems.push_back({t, name, n1, n2, val});
  }
  return elems;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    cerr << "Usage: dc_linear <netlist>\n";
    return 1;
  }
  std::string path = argv[1];
  auto elems = parse_netlist(path);

  // map nodes to indices, ground node '0' -> index 0 but we will exclude ground
  // from variable list
  unordered_map<std::string, int> node_map;
  node_map["0"] = 0;
  int next_node = 1;
  int num_voltage = 0;
  for (auto& e : elems) {
    if (node_map.find(e.n1) == node_map.end()) node_map[e.n1] = next_node++;
    if (node_map.find(e.n2) == node_map.end()) node_map[e.n2] = next_node++;
    if (e.type == 'V') num_voltage++;
  }

  int num_nodes = next_node;                     // includes ground
  int num_vars = (num_nodes - 1) + num_voltage;  // exclude ground voltage

  // mapping from node index to variable index (voltage). ground -> -1
  vector<int> node_to_var(num_nodes, -1);
  int var = 0;
  for (int i = 0; i < num_nodes; i++) {
    if (i == 0) {
      node_to_var[i] = -1;
      continue;
    }
    node_to_var[i] = var++;  // node voltages occupy 0..num_nodes-2
  }
  // assign voltage source extra var indices in order encountered
  vector<int> vsrc_extra;
  vsrc_extra.reserve(num_voltage);
  int extra_base = var;
  for (auto& e : elems)
    if (e.type == 'V') vsrc_extra.push_back(var++);

  // Build dense MNA matrix and RHS
  vector<vector<double>> A(num_vars, vector<double>(num_vars, 0.0));
  vector<double> z(num_vars, 0.0);

  // helper to get node numeric index
  auto nid = [&](const std::string& s) { return node_map[s]; };

  // iterate elements and stamp
  int vcount = 0;
  for (auto& e : elems) {
    int n1 = nid(e.n1);
    int n2 = nid(e.n2);
    if (e.type == 'R') {
      double g = 1.0 / e.value;
      int v1 = node_to_var[n1];
      int v2 = node_to_var[n2];
      if (v1 != -1) A[v1][v1] += g;
      if (v2 != -1) A[v2][v2] += g;
      if (v1 != -1 && v2 != -1) {
        A[v1][v2] -= g;
        A[v2][v1] -= g;
      }
    } else if (e.type == 'I') {
      double I = e.value;
      int v1 = node_to_var[n1];
      int v2 = node_to_var[n2];
      if (v1 != -1) z[v1] -= I;
      if (v2 != -1) z[v2] += I;
    } else if (e.type == 'V') {
      int k = vsrc_extra[vcount++];
      int v1 = node_to_var[n1];
      int v2 = node_to_var[n2];
      if (v1 != -1) A[v1][k] += 1.0;
      if (v2 != -1) A[v2][k] -= 1.0;
      if (v1 != -1) A[k][v1] += 1.0;
      if (v2 != -1) A[k][v2] -= 1.0;
      z[k] += e.value;
    }
  }

  // Solve linear system A x = z using Gaussian elimination with partial
  // pivoting
  int n = num_vars;
  vector<vector<double>> M = A;  // copy
  vector<double> b = z;
  vector<int> piv(n);
  for (int i = 0; i < n; i++) piv[i] = i;

  for (int k = 0; k < n; k++) {
    // find pivot
    int p = k;
    double maxv = std::fabs(M[k][k]);
    for (int i = k + 1; i < n; i++) {
      double v = std::fabs(M[i][k]);
      if (v > maxv) {
        maxv = v;
        p = i;
      }
    }
    if (maxv < 1e-15) {
      cerr << "Singular or ill-conditioned matrix (pivot ~ 0)\n";
      break;
    }
    if (p != k) {
      swap(M[p], M[k]);
      swap(b[p], b[k]);
    }
    double pivot = M[k][k];
    for (int j = k; j < n; j++) M[k][j] /= pivot;
    b[k] /= pivot;
    for (int i = 0; i < n; i++)
      if (i != k) {
        double fac = M[i][k];
        if (fac == 0.0) continue;
        for (int j = k; j < n; j++) M[i][j] -= fac * M[k][j];
        b[i] -= fac * b[k];
      }
  }

  vector<double> x(n, 0.0);
  for (int i = 0; i < n; i++) x[i] = b[i];

  // report results: node voltages and voltage-source currents
  cout << fixed << setprecision(6);
  // build reverse node map
  std::vector<std::string> rev_node(node_map.size());
  for (auto& kv : node_map) rev_node[kv.second] = kv.first;
  cout << "Solution:\n";
  for (int ni = 0; ni < num_nodes; ++ni) {
    if (ni == 0) {
      cout << " node 0 (gnd) = 0.000000 V\n";
      continue;
    }
    int vi = node_to_var[ni];
    cout << " node " << rev_node[ni] << " = " << x[vi] << " V\n";
  }
  // voltage source currents
  vcount = 0;
  for (auto& e : elems)
    if (e.type == 'V') {
      int k = vsrc_extra[vcount++];
      cout << " current(" << e.name << ") = " << x[k] << " A\n";
    }

  return 0;
}
