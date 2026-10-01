#include "app/park_file_requests.h"

#include <utility>

namespace tpj {

ParkDialog ParkFileRequests::press(ParkAction action) {
  const std::scoped_lock lock(Lock);
  if (DialogShowing || action == ParkAction::None) {
    return ParkDialog::None;
  }
  if (action == ParkAction::New) {
    Request = FileRequest{ParkAction::New, {}};
    return ParkDialog::None;
  }
  DialogShowing = true;
  return action == ParkAction::Open ? ParkDialog::Open : ParkDialog::Save;
}

void ParkFileRequests::answer(ParkDialog dialog, const char *chosen) {
  const std::scoped_lock lock(Lock);
  DialogShowing = false;
  if (chosen != nullptr && dialog != ParkDialog::None) {
    Request = FileRequest{dialog == ParkDialog::Open ? ParkAction::Open : ParkAction::Save, chosen};
  }
}

bool ParkFileRequests::dialogShowing() const {
  const std::scoped_lock lock(Lock);
  return DialogShowing;
}

FileRequest ParkFileRequests::take() {
  const std::scoped_lock lock(Lock);
  FileRequest request = std::move(Request);
  Request = FileRequest{};
  return request;
}

} // namespace tpj
