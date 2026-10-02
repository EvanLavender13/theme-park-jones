#ifndef TPJ_APP_SESSION_PARK_FILE_REQUESTS_H
#define TPJ_APP_SESSION_PARK_FILE_REQUESTS_H

#include <mutex>
#include <stdint.h>
#include <string>

namespace tpj {

// A park button the player pressed in the Tools panel.
enum class ParkAction : uint8_t { None, New, Open, Save };

// A park dialog to show, or none.
enum class ParkDialog : uint8_t { None, Open, Save };

// One request, taken by the main loop: New with no path, or Open or Save with the chosen path.
struct FileRequest {
  ParkAction Action = ParkAction::None;
  std::string Path;
};

// What the park buttons and the file dialogs ask of the main loop. A dialog's answer may come from
// another thread, so every call takes a lock.
class ParkFileRequests {
public:
  // A pressed button. With no dialog showing, New becomes the request, and Open or Save shows its
  // dialog, which it returns. While a dialog shows, or for None, it does nothing.
  ParkDialog press(ParkAction action);
  // A dialog's answer: no dialog shows from then on, and a chosen path, when there is one, becomes
  // the request with the dialog's action.
  void answer(ParkDialog dialog, const char *chosen);
  [[nodiscard]] bool dialogShowing() const;
  // The request, leaving none.
  FileRequest take();

private:
  mutable std::mutex Lock;
  bool DialogShowing = false;
  FileRequest Request;
};

} // namespace tpj

#endif
