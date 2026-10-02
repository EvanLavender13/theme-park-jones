#include "app/input/input_map.h"
#include "app/input/orbit_camera.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <span>
#include <stdint.h>

namespace tpj {
namespace {

SDL_Event quitEvent() {
  SDL_Event event{};
  event.type = SDL_EVENT_QUIT;
  return event;
}

SDL_Event buttonEvent(uint8_t button, bool down) {
  SDL_Event event{};
  event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
  event.button.button = button;
  event.button.down = down;
  return event;
}

SDL_Event motionEvent(SDL_MouseButtonFlags held, float xrel, float yrel) {
  SDL_Event event{};
  event.type = SDL_EVENT_MOUSE_MOTION;
  event.motion.state = held;
  event.motion.xrel = xrel;
  event.motion.yrel = yrel;
  return event;
}

SDL_Event wheelEvent(float steps, SDL_MouseWheelDirection direction = SDL_MOUSEWHEEL_NORMAL) {
  SDL_Event event{};
  event.type = SDL_EVENT_MOUSE_WHEEL;
  event.wheel.direction = direction;
  event.wheel.y = steps;
  return event;
}

SDL_Event keyDownEvent(SDL_Scancode scancode) {
  SDL_Event event{};
  event.type = SDL_EVENT_KEY_DOWN;
  event.key.scancode = scancode;
  event.key.down = true;
  return event;
}

// The fields the mouse drives.
void checkSameMouseFields(const CameraInput &actual, const CameraInput &expected) {
  CHECK(actual.OrbitDx == expected.OrbitDx);
  CHECK(actual.OrbitDy == expected.OrbitDy);
  CHECK(actual.PanDx == expected.PanDx);
  CHECK(actual.PanDy == expected.PanDy);
  CHECK(actual.Zoom == expected.Zoom);
}

// The fields the keys drive.
void checkSameKeyFields(const CameraInput &actual, const CameraInput &expected) {
  CHECK(actual.MoveForward == expected.MoveForward);
  CHECK(actual.MoveRight == expected.MoveRight);
  CHECK(actual.Rotate == expected.Rotate);
}

void checkSameCamera(const CameraInput &actual, const CameraInput &expected) {
  checkSameMouseFields(actual, expected);
  checkSameKeyFields(actual, expected);
}

// A camera input with every field set, so a change to any field shows.
constexpr CameraInput SET_CAMERA{.OrbitDx = 1.0f,
                                 .OrbitDy = 2.0f,
                                 .PanDx = 3.0f,
                                 .PanDy = 4.0f,
                                 .Zoom = 5.0f,
                                 .MoveForward = 0.5f,
                                 .MoveRight = -0.5f,
                                 .Rotate = 0.25f};

using Keys = std::array<bool, SDL_SCANCODE_COUNT>;

TEST_CASE("A quit event asks to quit whether or not ImGui wants the mouse") {
  for (const bool mouseWanted : {false, true}) {
    FrameInput input;
    mapEvent(quitEvent(), mouseWanted, input);
    CHECK(input.Quit);
  }
}

TEST_CASE("A left button release is recorded whether or not ImGui wants the mouse") {
  for (const bool mouseWanted : {false, true}) {
    FrameInput input;
    mapEvent(buttonEvent(SDL_BUTTON_LEFT, false), mouseWanted, input);
    CHECK(input.Buttons.Released);
    CHECK_FALSE(input.Buttons.Pressed);
  }
}

TEST_CASE("A left button press is recorded only while ImGui does not want the mouse") {
  FrameInput free;
  mapEvent(buttonEvent(SDL_BUTTON_LEFT, true), false, free);
  CHECK(free.Buttons.Pressed);
  CHECK_FALSE(free.Buttons.Released);

  FrameInput wanted;
  mapEvent(buttonEvent(SDL_BUTTON_LEFT, true), true, wanted);
  CHECK_FALSE(wanted.Buttons.Pressed);
  CHECK_FALSE(wanted.Buttons.Released);
}

TEST_CASE("No other event changes the request to quit or the buttons") {
  // The other buttons' presses and releases are the events most easily mistaken for the left's.
  const std::array<SDL_Event, 7> others{
      buttonEvent(SDL_BUTTON_RIGHT, true),
      buttonEvent(SDL_BUTTON_RIGHT, false),
      buttonEvent(SDL_BUTTON_MIDDLE, true),
      buttonEvent(SDL_BUTTON_MIDDLE, false),
      motionEvent(SDL_BUTTON_LMASK | SDL_BUTTON_RMASK, 5.0f, 6.0f),
      wheelEvent(1.0f),
      keyDownEvent(SDL_SCANCODE_ESCAPE),
  };
  for (const bool mouseWanted : {false, true}) {
    for (const SDL_Event &event : others) {
      FrameInput input;
      mapEvent(event, mouseWanted, input);
      CHECK_FALSE(input.Quit);
      CHECK_FALSE(input.Buttons.Pressed);
      CHECK_FALSE(input.Buttons.Released);
    }
  }
}

TEST_CASE("Motion with the right button held adds its relative pixels to the orbit deltas") {
  // The middle button held too does not divert it to the pan deltas.
  FrameInput input;
  mapEvent(motionEvent(SDL_BUTTON_RMASK, 3.0f, -2.0f), false, input);
  mapEvent(motionEvent(SDL_BUTTON_RMASK | SDL_BUTTON_MMASK, 4.0f, 7.0f), false, input);
  CameraInput expected;
  expected.OrbitDx = 7.0f;
  expected.OrbitDy = 5.0f;
  checkSameCamera(input.Camera, expected);
}

TEST_CASE("Motion with the middle button held and not the right adds its relative pixels to the "
          "pan deltas") {
  FrameInput input;
  mapEvent(motionEvent(SDL_BUTTON_MMASK, -6.0f, 2.0f), false, input);
  mapEvent(motionEvent(SDL_BUTTON_MMASK | SDL_BUTTON_LMASK, 1.0f, 3.0f), false, input);
  CameraInput expected;
  expected.PanDx = -5.0f;
  expected.PanDy = 5.0f;
  checkSameCamera(input.Camera, expected);
}

TEST_CASE("Motion with neither the right nor the middle button held moves no camera input") {
  FrameInput input;
  mapEvent(motionEvent(0, 8.0f, 9.0f), false, input);
  mapEvent(motionEvent(SDL_BUTTON_LMASK, 8.0f, 9.0f), false, input);
  checkSameCamera(input.Camera, CameraInput{});
}

TEST_CASE("A wheel event adds its vertical steps to the zoom, its y as SDL gives it whatever its "
          "direction") {
  FrameInput input;
  mapEvent(wheelEvent(2.0f), false, input);
  mapEvent(wheelEvent(-0.5f, SDL_MOUSEWHEEL_FLIPPED), false, input);
  mapEvent(wheelEvent(1.0f, SDL_MOUSEWHEEL_FLIPPED), false, input);
  CameraInput expected;
  expected.Zoom = 2.5f;
  checkSameCamera(input.Camera, expected);
}

TEST_CASE("While ImGui wants the mouse no event changes the camera input") {
  FrameInput input;
  input.Camera = SET_CAMERA;
  mapEvent(motionEvent(SDL_BUTTON_RMASK, 3.0f, 4.0f), true, input);
  mapEvent(motionEvent(SDL_BUTTON_MMASK, 3.0f, 4.0f), true, input);
  mapEvent(wheelEvent(1.0f), true, input);
  checkSameCamera(input.Camera, SET_CAMERA);
}

TEST_CASE("mapKeys sets each axis to its positive key's state minus its negative key's") {
  struct Axis {
    SDL_Scancode Positive;
    SDL_Scancode Negative;
    float CameraInput::*Field;
  };
  const std::array<Axis, 3> axes{
      Axis{SDL_SCANCODE_W, SDL_SCANCODE_S, &CameraInput::MoveForward},
      Axis{SDL_SCANCODE_D, SDL_SCANCODE_A, &CameraInput::MoveRight},
      Axis{SDL_SCANCODE_Q, SDL_SCANCODE_E, &CameraInput::Rotate},
  };
  for (const Axis &axis : axes) {
    Keys positive{};
    positive.at(axis.Positive) = true;
    Keys negative{};
    negative.at(axis.Negative) = true;
    Keys both{};
    both.at(axis.Positive) = true;
    both.at(axis.Negative) = true;

    // Starting from set axes shows mapKeys sets them rather than adding to them.
    CameraInput camera = SET_CAMERA;
    mapKeys(positive, false, camera);
    CHECK(camera.*axis.Field == 1.0f);
    camera = SET_CAMERA;
    mapKeys(negative, false, camera);
    CHECK(camera.*axis.Field == -1.0f);
    camera = SET_CAMERA;
    mapKeys(both, false, camera);
    CHECK(camera.*axis.Field == 0.0f);
    camera = SET_CAMERA;
    mapKeys(Keys{}, false, camera);
    CHECK(camera.*axis.Field == 0.0f);
  }
}

TEST_CASE("mapKeys changes no field of the camera input but the move and rotate axes") {
  // The main loop gathers the mouse's deltas with mapEvent before it maps the keys.
  Keys keys{};
  keys.at(SDL_SCANCODE_W) = true;
  keys.at(SDL_SCANCODE_A) = true;
  keys.at(SDL_SCANCODE_Q) = true;
  CameraInput camera = SET_CAMERA;
  mapKeys(keys, false, camera);
  checkSameMouseFields(camera, SET_CAMERA);
}

TEST_CASE("A scancode past the end of the keys counts as not held") {
  // The keys end just after S, so W lies past them while S is held.
  std::array<bool, SDL_SCANCODE_S + 1> keys{};
  keys.at(SDL_SCANCODE_S) = true;
  CameraInput camera = SET_CAMERA;
  mapKeys(keys, false, camera);
  CHECK(camera.MoveForward == -1.0f);

  camera = SET_CAMERA;
  mapKeys(std::span<const bool>{}, false, camera);
  CHECK(camera.MoveForward == 0.0f);
  CHECK(camera.MoveRight == 0.0f);
  CHECK(camera.Rotate == 0.0f);
}

TEST_CASE("While ImGui wants the keyboard mapKeys changes nothing") {
  Keys keys{};
  keys.at(SDL_SCANCODE_W) = true;
  keys.at(SDL_SCANCODE_A) = true;
  keys.at(SDL_SCANCODE_Q) = true;
  CameraInput camera = SET_CAMERA;
  mapKeys(keys, true, camera);
  checkSameCamera(camera, SET_CAMERA);
}

} // namespace
} // namespace tpj
