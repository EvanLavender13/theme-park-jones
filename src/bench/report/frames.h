#ifndef TPJ_BENCH_REPORT_FRAMES_H
#define TPJ_BENCH_REPORT_FRAMES_H

#include <string>
#include <string_view>

namespace tpj {

// The launch text of the app's output on a park: `park <park> ticks 0 warm-ups 1 repetitions <k>`
// and a frames stage summarizing its frame-times line's k durations. Throws ReportError unless the
// output holds exactly one frame-times line, with at least one duration, each a time.
std::string framesLaunch(std::string_view park, std::string_view output);

} // namespace tpj

#endif
