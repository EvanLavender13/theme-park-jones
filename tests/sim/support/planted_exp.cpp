#include <cmath>

// The one call to the C runtime's exp in tpj_planted_exp, an archive the symbol check must reject.
// Nothing links it. C linkage keeps the name the check reports unmangled.
extern "C" double tpjPlantedExp(double x) { return std::exp(x); }
