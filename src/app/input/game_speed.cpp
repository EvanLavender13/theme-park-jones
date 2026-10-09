#include "app/input/game_speed.h"

namespace tpj {

GameSpeed::GameSpeed(uint32_t speed) : Running(speed == 0 ? 1 : speed), Paused(speed == 0) {}

uint32_t GameSpeed::multiplier() const { return Paused ? 0 : Running; }

void GameSpeed::pause() { Paused = true; }

void GameSpeed::run(uint32_t speed) {
  Running = speed;
  Paused = false;
}

void GameSpeed::use(SpeedAction action) {
  switch (action) {
  case SpeedAction::None:
    break;
  case SpeedAction::TogglePause:
    Paused = !Paused;
    break;
  case SpeedAction::RunNormal:
    run(1);
    break;
  case SpeedAction::RunDouble:
    run(2);
    break;
  case SpeedAction::RunQuadruple:
    run(4);
    break;
  }
}

} // namespace tpj
