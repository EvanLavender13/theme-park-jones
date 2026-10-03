# Research: budgeted-report

## How should the app's own frames be timed?

What the player feels is the time from one frame to the next, which includes the ticks a frame steps, the meshes it rebuilds, the panels, and the draw and present. The frame clock already reads the performance counter once at each frame's start, so the difference between consecutive readings is each frame's whole cost, with no second clock and nothing inside the frame timed. The clamped Dt cannot serve, since it stops at 0.25 s, exactly the frames that matter. Since the reading at a frame's start closes the previous frame, the second reading gives the first frame's cost, and the first frame is not ordinary play: it follows loading, builds and uploads the park mesh, frames the camera, and runs the first ImGui frame and present. Dropping the first two readings' durations keeps loading and that one-off work out, so the worst frame reported is a play hitch, not startup. Timing full.park this way by hand, through --frames and the shell's clock, gave about 320 ms a frame before the tick cap and about 100 ms after it, with loading under half a second.

Rejected: timing the whole launch with the shell's clock and subtracting a one-frame launch — mixes loading, window creation, and shutdown into every frame and hides the worst frame. Timing inside the app with std::chrono — a second clock beside the counter the frame clock already reads. Computing the median and spread in the app — duplicates tpj_bench_report's summary rule in shipped code; the app writes its durations and the report summarizes them like any launch. Choosing the kept frames inline in the Application's loop — untestable without a window; a small component keeps them and is tested through its header (decision 0027).

## How long must a stress run be?

The ticks stage stepped 300 ticks, ten seconds of game time, and reported a median. The full park's guests stay 36,000 ticks past its save, so a two-minute run of 3,600 ticks stays within a full park. At 44 ms a tick, 3,600 ticks take about 160 s a launch on windows-release, so a ten-launch report takes about half an hour before the index and a few minutes after it. windows-debug keeps 300 ticks on its parks, the same in every report.

## What marks a stage as over budget?

A tick covers SIM_TICK_SECONDS, 1/30 s, and the app steps up to two ticks a frame, so any stage that alone takes longer than one tick's time, whether a tick, an overlay rebuild done at a tick, or a frame, stalls play. The budget is therefore 33,333,333 ns for every stage, the whole nanoseconds of SIM_TICK_SECONDS, and a stage is over it when its report median exceeds it. The worst call across launches is reported beside the median, so a hitch shows even when the median is within budget.

Sources: src/app/frame_clock.cpp; src/app/application.cpp; src/scenarios/SPEC.md (FULL_PARK_STAY); timing of build/windows-release/ThemeParkJones.exe --park tests/parks/stress/full.park --frames 60 and 300 before and after 66d00c6.
