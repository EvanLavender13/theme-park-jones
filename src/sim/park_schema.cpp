#include "sim/park_schema.h"

#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"

namespace tpj {

void addPark(WorldSchema &schema) {
  // The medium's types come first, since every capability builds on them. Capabilities register
  // their park types after them, in dependency order, starting with park intent and its commands,
  // then the routes that derive the park's networks, then the operations that run its shops and
  // depots.
  addNetworkComponent(schema);
  addParkIntent(schema);
  addParkEdits(schema);
  addRoutes(schema);
  addOperations(schema);
}

std::shared_ptr<const WorldSchema> makeParkSchema() {
  auto schema = std::make_shared<WorldSchema>();
  addPark(*schema);
  return schema;
}

} // namespace tpj
