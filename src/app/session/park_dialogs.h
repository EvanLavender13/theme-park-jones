#ifndef TPJ_APP_SESSION_PARK_DIALOGS_H
#define TPJ_APP_SESSION_PARK_DIALOGS_H

#include "app/session/park_file_requests.h"
#include "app/session/park_session.h"

#include <SDL3/SDL.h>

#include <string>

namespace tpj {

// The park file flow's platform edge: SDL's open and save dialogs for the window, which answer the
// program's one ParkFileRequests, and the message boxes that ask before a replace and show errors.
class ParkDialogs final : public ParkFileEdge {
public:
  explicit ParkDialogs(SDL_Window *window);

  // Acts on a park button: New waits for the next frame, and Open and Save show their dialog,
  // filtered to .park files and starting in the parks folder beside the executable. Does nothing
  // while a dialog is showing.
  void press(ParkAction action);
  [[nodiscard]] bool dialogShowing() const;
  // The request the buttons and dialogs made, leaving none.
  FileRequest take();

  bool confirmReplace(const std::string &path) override;
  void reportError(const std::string &message) override;

private:
  SDL_Window *Window;
  // The program's one mailbox, which outlives every ParkDialogs.
  ParkFileRequests &Requests;
};

} // namespace tpj

#endif
