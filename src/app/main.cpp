#include "app/application.h"
#include "app/options.h"
#include "app/session/park_session.h"
#include "sim/world.h"

#include <SDL3/SDL_main.h>

#include <optional>
#include <stdio.h>
#include <stdlib.h>
#include <utility>

int main(int argc, char **argv) {
  const std::optional<tpj::Options> options = tpj::parseOptions(argc, argv);
  if (!options) {
    return EXIT_FAILURE;
  }
  tpj::OpenedPark start = tpj::startingPark(options->ParkPath, options->Ticks);
  if (!start.Park) {
    (void)fprintf(stderr, "%s\n", start.Error.c_str());
    return EXIT_FAILURE;
  }
  // The hash needs no window, so it can be checked where there is no display.
  if (options->PrintHash) {
    printf("tick %llu hash %016llx\n", static_cast<unsigned long long>(start.Park->Tick),
           static_cast<unsigned long long>(tpj::hashWorld(*start.Park)));
    return EXIT_SUCCESS;
  }
  tpj::Application app(*options, std::move(*start.Park));
  return app.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
