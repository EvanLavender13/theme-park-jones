# Research: graph-view

## How are the networks' lines put on the screen?

The view projects ground points with the matrices drawFrame uses, perspective after lookAt, and divides by w to reach normalized device coordinates, then maps those to window coordinates with y flipped, since the window's y grows downward. Dear ImGui's draw lists take positions in the display's units, io.DisplaySize, which the SDL3 backend sets to the window's size in points, while the render target is in pixels; the aspect ratio is the same, and ImGui's framebuffer scale maps points to pixels when it renders. So the overlay is built for io.DisplaySize and lands where the scene draws the ground. Projecting with the same matrices as the scene and as picking also means the overlay and groundAtCursor agree: a window point turned back into device coordinates gives the ground point it came from, so the graph is drawn where a click would land.

## What happens to a segment that passes behind the camera?

A point behind the eye has a negative w, and dividing by it throws the point to the opposite side of the screen, so a segment with one end behind the camera would be drawn as a line shooting across the view. The standard remedy is to clip before the divide, against the near plane: keep the part of the segment whose view depth is at least the near distance, replacing the far end with the point where the segment crosses that plane. View depth, the distance along the view direction, is w in this projection, and it is linear along a segment in world space, so the crossing is at t = (near - d_a) / (d_b - d_a) of the way from a to b, and the clipped point can be found on the ground and projected like any other. A segment with both ends in front of the near plane is kept whole, and one with both behind it is dropped, since its depth never reaches the plane between them. Nothing needs clipping against the sides or the far plane: ImGui clips what it draws to the display, and the park's 256 m square lies well inside the 2000 m far plane.

Rejected: dropping any segment with an end behind the camera. With the orbit camera low over a long path, the path would vanish near the viewer. Clipping in clip space, interpolating the four clip coordinates. It gives the same point, since the projection is linear before the divide, but the ground-space form lets the clipped end go through the same projection as every other point.

## How is the view drawn?

ImGui's background draw list draws behind every ImGui window and over the rendered scene, with no depth test, so the graph is visible through boxes and ribbons, which suits a view for checking the networks. AddLine draws a segment of a thickness, and AddCircleFilled a node. Building the overlay as plain data, lines and nodes in window coordinates, keeps the projection and clipping testable without a window or GPU, as the park mesh is built on the CPU, while the app only hands the data to the draw list.

Sources: https://chaosinmotion.com/2016/05/22/3d-clipping-in-homogeneous-coordinates/ — clipping against the near plane before the perspective divide; https://paroj.github.io/gltut/Positioning/Tut05%20Boundaries%20and%20Clipping.html — why a negative w flips a point across the screen; https://github.com/ocornut/imgui/issues/545 — the background draw list for overlays on a 3D scene.
