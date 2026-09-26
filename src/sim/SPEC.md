# sim

The simulation: the park's state and the rules that change it. Currently a skeleton that holds only a tick counter.

## Contract

The target tpj_sim links nothing but tpj_core and EnTT. It must never depend on rendering, windowing, or input (principle 10), and the test suite links it alone, so a violation fails the build. Entities will live in an EnTT registry once the first entity kind exists; each kind's internal components are private to its module, and only shared-medium components are public (decision 0016).

The world advances only through stepWorld, one fixed tick of SIM_TICK_SECONDS (1/30 s) at a time. Frame rate never changes what a tick does. Each call to stepWorld increments World::Tick by one.

Stepping is deterministic: the same world state stepped the same number of times gives the same result, bit for bit, on every supported build (decision 0022). Simulation code therefore never calls the C runtime's transcendental math functions, and tpj_sim builds with -ffp-contract=off.
