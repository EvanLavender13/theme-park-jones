// tpj_bench: times each stage of the gameplay runtime on a park file, writing each stage's times
// beside its result.
//
//   tpj_bench [--ticks N] FILE

#include "bench/options.h"
#include "bench/park_file.h"
#include "bench/stages.h"
#include "sim/field_text.h"
#include "sim/park_schema.h"
#include "sim/save.h"

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  const std::vector<std::string> arguments(argv, argv + argc);
  std::string error;
  const std::optional<tpj::BenchOptions> options = tpj::parseBenchOptions(arguments, error);
  if (!options) {
    std::cerr << "tpj_bench: " << error << "\nusage: tpj_bench [--ticks N] FILE\n";
    return 2;
  }
  const std::optional<std::string> text = tpj::readParkFile(options->Park);
  if (!text) {
    std::cerr << "tpj_bench: cannot read " << options->Park << '\n';
    return 1;
  }
  try {
    const tpj::World loaded = tpj::loadWorld(tpj::makeParkSchema(), *text);
    const std::vector<tpj::StageResult> stages = tpj::benchPark(loaded, options->Ticks);
    std::cout << tpj::parkLine(options->Park, options->Ticks) << '\n';
    for (const tpj::StageResult &stage : stages) {
      std::cout << tpj::stageLine(stage) << '\n';
    }
  } catch (const tpj::LoadError &loadError) {
    std::cerr << "tpj_bench: cannot load " << options->Park << ": " << loadError.what() << '\n';
    return 1;
  } catch (const std::exception &failure) {
    std::cerr << "tpj_bench: " << failure.what() << '\n';
    return 1;
  }
  return 0;
}
