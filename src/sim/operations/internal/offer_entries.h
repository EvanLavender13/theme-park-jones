#ifndef TPJ_SIM_OPERATIONS_INTERNAL_OFFER_ENTRIES_H
#define TPJ_SIM_OPERATIONS_INTERNAL_OFFER_ENTRIES_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/operations/operations.h"

#include <stdint.h>
#include <vector>

namespace tpj {

class World;

// The source's entries of the offer: one at each node anchored to it on the guest network.
std::vector<PlacedEntry<OfferEntry>> offerEntries(const World &world, EntityKey source,
                                                  const OfferEntry &offer);
// Appends the same entries to entries, using nodes as its list of the source's nodes. Allocates
// nothing once both lists have grown.
void fillOfferEntries(const World &world, EntityKey source, const OfferEntry &offer,
                      std::vector<uint32_t> &nodes, std::vector<PlacedEntry<OfferEntry>> &entries);

} // namespace tpj

#endif
