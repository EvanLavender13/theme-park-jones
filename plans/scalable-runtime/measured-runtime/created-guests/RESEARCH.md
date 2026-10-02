# Research: created-guests

## How can arrivals create their guests through addGuest without changing a single draw?

An arriving guest draws its stay, its hunger rate, and its starting hunger, each with drawUniform(drawKey(world, the guest's key, hashName(purpose), 0)). drawKey takes the world's seed and tick. addGuest(world, place, stayUntil) takes the stay as an argument and creates the entity itself, so arrivals must draw the stay on a key that does not exist yet. A key from createEntity comes from the world's counter (src/sim/SPEC.md), and World::nextKey() gives the key the next createEntity will return. So arrivals can draw the stay on world.nextKey() and then call addGuest. The guest then gets the key the stay was drawn on, its hunger and hunger rate are drawn by addGuest as before, and every number an arrival produces is unchanged. The tests already lean on the same fact: tests/sim/support/guest_parks.h names a coming guest as EntityKey{world.nextKey()}.

Every draw is a pure function of its key, so the order of the draws does not matter. Drawing the stay in admitGuests and the hunger in addGuest gives the same values as drawing all three in one place. The arithmetic is the same expressions in the same translation unit, built with tpj_sim's floating-point flags, so the cross-build check's output stays identical.

Rejected: drawing the stay on a key other than the guest's, such as the entrance's — it changes every arriving guest's stay, so the cross-build check's output and the existing arrival tests would change, and the milestone changes no observable behavior. Letting addGuest draw the stay when it is given none — it gives addGuest two contracts, and the generator, which must pick stays past the runner's longest run, would never use the drawn one. Having arrivals overwrite StayUntil after addGuest returns — the guest would come into being in two steps, and the milestone requires one way. A private helper shared by addGuest and arrivals — arrivals would then not go through addGuest, so the public function and the real arrivals could drift apart unseen.

## Should addGuest accept a place that is not on the guest network?

A guest whose place does not resolve is a legitimate state (principle 2): carrying leaves one when the guest network loses every carrier, and it leaves the park when it next steps. But a caller that creates one on purpose has made a mistake, and it would show only as a guest silently gone a tick later. A generator that put 2,000 guests off the network would make a park that looks right and holds nobody. softmaxPick and the draw functions already throw std::invalid_argument and change nothing for input they cannot honor, so addGuest does the same for a place that does not resolve on parkNetwork(world, PathKind::Guest). Before a world's first resolution that network is empty, so addGuest refuses every place until the world is resolved.

Rejected: accepting any place — the mistake shows up only as guests vanishing a tick later. Refusing while a resolution is pending — after an edit the network is the last resolution's, the network before that the next resolution carries guests from, so a guest added then is carried like every other.

Sources: src/sim/guests/guests.cpp (admitGuests); src/sim/world.h (nextKey); src/sim/SPEC.md (keys from the counter); tests/sim/support/guest_parks.h.
