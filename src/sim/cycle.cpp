#include "sim/world.h"

#include "core/profile.h"

#include <stdexcept>
#include <string>

namespace tpj {

namespace {

// Throws before anything changes when a command's type is not registered with the world's schema.
void requireRegistered(const World &world, const CommandQueue &commands) {
  for (const QueuedCommand &command : commands.commands()) {
    if (world.schema().findCommand(command.TypeId) == nullptr) {
      throw std::invalid_argument("command type " + std::string(command.TypeName) +
                                  " is not registered with the world's schema");
    }
  }
}

void applyCommands(World &world, const CommandQueue &commands) {
  for (const QueuedCommand &command : commands.commands()) {
    world.schema().findCommand(command.TypeId)->Apply(world, command.Value);
  }
}

} // namespace

void resolveWorld(World &world) {
  TPJ_PROFILE_ZONE();
  world.Resolving = true;
  try {
    for (const ResolverType &resolver : world.schema().resolvers()) {
      resolver.Resolve(world);
    }
  } catch (...) {
    world.Resolving = false;
    throw;
  }
  world.Resolving = false;
  // Finishers run with isResolving false, so they cannot publish after the comparisons they make.
  world.Finishing = true;
  try {
    for (const WorldFunction finish : world.schema().finishers()) {
      finish(world);
    }
  } catch (...) {
    world.Finishing = false;
    throw;
  }
  world.Finishing = false;
  world.ResolvePending = false;
}

void stepWorld(World &world, CommandQueue &commands) {
  TPJ_PROFILE_ZONE();
  requireRegistered(world, commands);
  if (world.ResolvePending) {
    resolveWorld(world);
  }
  world.Stepping = true;
  try {
    for (const WorldFunction step : world.schema().systems()) {
      step(world);
    }
  } catch (...) {
    world.Stepping = false;
    throw;
  }
  world.Stepping = false;
  ++world.Tick;
  for (const WorldFunction swap : world.schema().swaps()) {
    swap(world);
  }
  if (!commands.empty()) {
    applyCommands(world, commands);
    world.ResolvePending = true;
    commands.clear();
  }
  if (world.ResolvePending) {
    resolveWorld(world);
  }
}

void stepWorld(World &world) {
  CommandQueue none;
  stepWorld(world, none);
}

World makeCandidate(const World &world, const CommandQueue &commands) {
  TPJ_PROFILE_ZONE();
  requireRegistered(world, commands);
  World candidate = copyWorld(world);
  if (!commands.empty()) {
    applyCommands(candidate, commands);
    candidate.ResolvePending = true;
  }
  if (candidate.ResolvePending) {
    resolveWorld(candidate);
  }
  return candidate;
}

} // namespace tpj
