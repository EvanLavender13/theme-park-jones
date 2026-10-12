#include "render/guest_mesh.h"

#include "sim/furniture/furniture.h"
#include "sim/guests/guests.h"

#include <algorithm>
#include <optional>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {

Rgba guestColor(double need) {
  const auto n = static_cast<float>(std::clamp(need, 0.0, 1.0));
  const auto mix = [n](float sated, float distressed) { return sated + (distressed - sated) * n; };
  return {mix(GUEST_SATED_COLOR.R, GUEST_DISTRESSED_COLOR.R),
          mix(GUEST_SATED_COLOR.G, GUEST_DISTRESSED_COLOR.G),
          mix(GUEST_SATED_COLOR.B, GUEST_DISTRESSED_COLOR.B), 1.0f};
}

Pose guestPose(GroundPoint point) {
  Pose pose;
  pose.X = point.X;
  pose.Z = point.Z;
  return pose;
}

void appendGuest(ParkMesh &mesh, GroundPoint point, double need) {
  appendBox(mesh, guestPose(point), GUEST_SIZE, GUEST_HEIGHT, guestColor(need));
}

namespace {

// Adds the guest's box where it is drawn, on its seat or standing, in the color.
void appendGuestBox(ParkMesh &mesh, const SeatedGuests &seated, EntityKey key,
                    const GuestRecord &record, Rgba color) {
  if (const std::optional<Pose> seat = seated.seatOf(key)) {
    appendBox(mesh, *seat, GUEST_SIZE, GUEST_SEATED_HEIGHT, color);
  } else if (record.Position) {
    appendBox(mesh, guestPose(*record.Position), GUEST_SIZE, GUEST_HEIGHT, color);
  }
}

} // namespace

void SeatedGuests::update(const World &world) {
  std::vector<Taken> next;
  std::vector<std::pair<EntityKey, SeatPlace>> newcomers;
  for (const EntityKey key : parkGuests(world)) {
    const std::optional<GuestRecord> record = guestRecord(world, key);
    if (!record || !record->Seat) {
      continue;
    }
    const SeatPlace &place = *record->Seat;
    const auto old = std::ranges::lower_bound(Seats, key, {}, &Taken::Guest);
    if (old != Seats.end() && old->Guest == key && old->Piece == place.Piece &&
        old->Seat < place.Seats) {
      next.push_back({key, place.Piece, old->Seat, seatPose(place.Kind, place.At, old->Seat)});
    } else {
      newcomers.emplace_back(key, place);
    }
  }
  // Guests already seated keep their seats, so newcomers take the lowest seats left.
  for (const auto &[key, place] : newcomers) {
    for (uint32_t seat = 0; seat < place.Seats; ++seat) {
      const bool taken = std::ranges::any_of(next, [&place, seat](const Taken &other) {
        return other.Piece == place.Piece && other.Seat == seat;
      });
      if (!taken) {
        next.push_back({key, place.Piece, seat, seatPose(place.Kind, place.At, seat)});
        break;
      }
    }
  }
  std::ranges::sort(next, {}, &Taken::Guest);
  Seats = std::move(next);
}

std::optional<Pose> SeatedGuests::seatOf(EntityKey guest) const {
  const auto found = std::ranges::lower_bound(Seats, guest, {}, &Taken::Guest);
  if (found == Seats.end() || found->Guest != guest) {
    return std::nullopt;
  }
  return found->At;
}

void appendGuestEntity(ParkMesh &mesh, const World &world, const SeatedGuests &seated,
                       EntityKey key, Rgba color) {
  if (const std::optional<GuestRecord> record = guestRecord(world, key)) {
    appendGuestBox(mesh, seated, key, *record, color);
  }
}

ParkMesh buildGuestMesh(const World &world, const SeatedGuests &seated) {
  ParkMesh mesh;
  for (const EntityKey key : parkGuests(world)) {
    if (const std::optional<GuestRecord> record = guestRecord(world, key)) {
      appendGuestBox(mesh, seated, key, *record,
                     guestColor(std::max(
                         {record->Hunger, record->Thirst, record->Tiredness, record->Bladder})));
    }
  }
  return mesh;
}

} // namespace tpj
