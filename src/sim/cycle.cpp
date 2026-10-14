#include "sim/world.h"

#include "core/profile.h"
#include "sim/unit_space.h"

#include <stdexcept>
#include <string>

namespace tpj {

namespace {

// Throws before anything changes when a command's type is not registered with the world's schema.
void requireRegistered(const World &world, const CommandQueue &commands) {
  for (const QueuedCommand &command : commands.commands()) {
    if (world.schema().findCommand(command.Id) == nullptr) {
      throw std::invalid_argument(
          "a queued command's type is not registered with the world's schema");
    }
  }
}

void applyCommands(World &world, const CommandQueue &commands) {
  for (const QueuedCommand &command : commands.commands()) {
    world.schema().findCommand(command.Id)->Apply(world, command.Value);
  }
}

} // namespace

void resolveWorld(World &world, TickBuffers &buffers) {
  TPJ_PROFILE_ZONE();
  // The changes stay on the list through the whole resolution, so later steps see what earlier
  // steps changed.
  UnitSpace &units = unitSpaceOf(world);
  units.beginResolution();
  world.Resolving = true;
  try {
    for (const ResolverType &resolver : world.schema().resolvers()) {
      resolver.Resolve(world, buffers);
    }
  } catch (...) {
    world.Resolving = false;
    throw;
  }
  world.Resolving = false;
  // Finishers run with isResolving false, so they cannot publish after the comparisons they make.
  world.Finishing = true;
  try {
    for (const ResolveFunction finish : world.schema().finishers()) {
      finish(world, buffers);
    }
  } catch (...) {
    world.Finishing = false;
    throw;
  }
  world.Finishing = false;
  world.ResolvePending = false;
  units.endResolution();
}

void resolveWorld(World &world) {
  TickBuffers buffers;
  resolveWorld(world, buffers);
}

void stepWorld(World &world, CommandQueue &commands, TickBuffers &buffers) {
  TPJ_PROFILE_ZONE();
  requireRegistered(world, commands);
  if (world.ResolvePending) {
    resolveWorld(world, buffers);
  }
  world.Stepping = true;
  try {
    for (const StepFunction step : world.schema().systems()) {
      step(world, buffers);
    }
  } catch (...) {
    world.Stepping = false;
    throw;
  }
  world.Stepping = false;
  ++world.Tick;
  for (const StepFunction swap : world.schema().swaps()) {
    swap(world, buffers);
  }
  if (!commands.empty()) {
    applyCommands(world, commands);
    world.ResolvePending = true;
    commands.clear();
  }
  if (world.ResolvePending) {
    resolveWorld(world, buffers);
  }
}

void stepWorld(World &world, TickBuffers &buffers) {
  CommandQueue none;
  stepWorld(world, none, buffers);
}

void stepWorld(World &world, CommandQueue &commands) {
  TickBuffers buffers;
  stepWorld(world, commands, buffers);
}

void stepWorld(World &world) {
  TickBuffers buffers;
  stepWorld(world, buffers);
}

World makeCandidate(const World &world, const CommandQueue &commands, TickBuffers &buffers) {
  TPJ_PROFILE_ZONE();
  requireRegistered(world, commands);
  World candidate = copyWorld(world);
  if (!commands.empty()) {
    applyCommands(candidate, commands);
    candidate.ResolvePending = true;
  }
  if (candidate.ResolvePending) {
    resolveWorld(candidate, buffers);
  }
  return candidate;
}

void makeCandidate(const World &world, const CommandQueue &commands, TickBuffers &buffers,
                   World &into) {
  TPJ_PROFILE_ZONE();
  requireRegistered(world, commands);
  copyWorld(world, into);
  if (!commands.empty()) {
    applyCommands(into, commands);
    into.ResolvePending = true;
  }
  if (into.ResolvePending) {
    resolveWorld(into, buffers);
  }
}

World makeCandidate(const World &world, const CommandQueue &commands) {
  TickBuffers buffers;
  return makeCandidate(world, commands, buffers);
}

void commitCommands(World &world, CommandQueue &commands, TickBuffers &buffers) {
  TPJ_PROFILE_ZONE();
  requireRegistered(world, commands);
  if (!commands.empty()) {
    applyCommands(world, commands);
    world.ResolvePending = true;
    commands.clear();
  }
  if (world.ResolvePending) {
    resolveWorld(world, buffers);
  }
}

void commitCommands(World &world, CommandQueue &commands) {
  TickBuffers buffers;
  commitCommands(world, commands, buffers);
}

} // namespace tpj
