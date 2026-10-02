#ifndef TPJ_BENCH_REPORT_REPORT_ERROR_H
#define TPJ_BENCH_REPORT_REPORT_ERROR_H

#include <stdexcept>

namespace tpj {

// Text that breaks the form of launches or of a report, or launches that cannot be summarized
// together. A reader's message begins `line <n>: `.
class ReportError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

} // namespace tpj

#endif
