# app

The executable: owns the window, the main loop, and input, and connects the simulation to the renderer.

## Main loop

Each frame gathers input, advances the simulation by as many fixed ticks as the elapsed time covers, updates the camera, and draws. Elapsed time per frame is clamped to 0.25 s so a stall does not trigger a burst of ticks.

## Camera

An orbit camera around a focus point on the ground.

- Right mouse drag orbits: horizontal changes yaw, vertical changes pitch.
- Middle mouse drag pans the focus across the ground, scaled by distance so the ground moves at a steady rate under the cursor.
- The mouse wheel zooms by a constant factor per step.
- W, A, S, and D move the focus relative to the view direction, at a speed proportional to distance.
- Q and E rotate the view around the focus.

Pitch stays between about 10 and 85 degrees, distance between 4 and 400 m, and the focus inside the park's square bounds.

## Command line

--frames N exits after N frames. --capture PATH writes the last frame to PATH as a BMP and exits after 3 frames unless --frames says otherwise. Together they make the app usable for automated visual checks.
