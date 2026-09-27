#ifndef TPJ_SIM_SIM_MATH_H
#define TPJ_SIM_SIM_MATH_H

namespace tpj {

// e^x, ported from musl's exp: within one ULP of the correctly rounded result, and the same bits
// on every build. Simulation code calls it, never std::exp.
double simExp(double x);

// The natural logarithm, ported from musl's log: within one ULP of the correctly rounded result,
// and the same bits on every build. Simulation code calls it, never std::log.
double simLog(double x);

} // namespace tpj

#endif
