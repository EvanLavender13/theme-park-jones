#ifndef TPJ_APP_INPUT_GAME_SPEED_H
#define TPJ_APP_INPUT_GAME_SPEED_H

#include <stdint.h>

namespace tpj {

// What the player asked of the game's speed this frame: to pause or unpause, or to run at 1x, 2x,
// or 4x.
enum class SpeedAction : uint8_t { None, TogglePause, RunNormal, RunDouble, RunQuadruple };

// Whether the park is paused, and the speed it runs at, or will run at when unpaused: 1, 2, or 4
// ticks for each tick of real time.
class GameSpeed {
public:
  // Paused at 1x when speed is 0, and otherwise running at speed, which is 1, 2, or 4.
  explicit GameSpeed(uint32_t speed);

  // The ticks the park steps for each tick of real time: 0 while paused.
  [[nodiscard]] uint32_t multiplier() const;
  [[nodiscard]] bool paused() const { return Paused; }
  // The speed it runs at, or will when unpaused.
  [[nodiscard]] uint32_t running() const { return Running; }

  // Pauses, keeping the speed it ran at.
  void pause();
  // Unpauses at speed, which is 1, 2, or 4.
  void run(uint32_t speed);
  // TogglePause pauses, or unpauses at the speed it ran at. RunNormal, RunDouble, and RunQuadruple
  // run at 1, 2, and 4. None changes nothing.
  void use(SpeedAction action);

private:
  uint32_t Running = 1;
  bool Paused = false;
};

} // namespace tpj

#endif
