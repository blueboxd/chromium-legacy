// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/player_tracker_impl.h"

#include <utility>

#include "base/stl_util.h"

namespace media {

PlayerTrackerImpl::PlayerCallbacks::PlayerCallbacks(
    base::RepeatingClosure new_key_cb,
    base::RepeatingClosure cdm_unset_cb)
    : new_key_cb(std::move(new_key_cb)),
      cdm_unset_cb(std::move(cdm_unset_cb)) {}

PlayerTrackerImpl::PlayerCallbacks::PlayerCallbacks(PlayerCallbacks&& other) =
    default;

PlayerTrackerImpl::PlayerCallbacks& PlayerTrackerImpl::PlayerCallbacks::
operator=(PlayerCallbacks&&) = default;

PlayerTrackerImpl::PlayerCallbacks::~PlayerCallbacks() = default;

PlayerTrackerImpl::PlayerTrackerImpl() : next_registration_id_(1) {
}

PlayerTrackerImpl::~PlayerTrackerImpl() = default;

int PlayerTrackerImpl::RegisterPlayer(base::RepeatingClosure new_key_cb,
                                      base::RepeatingClosure cdm_unset_cb) {
  base::AutoLock lock(lock_);
  int registration_id = next_registration_id_++;
  DCHECK(!base::Contains(player_callbacks_map_, registration_id));
  player_callbacks_map_.insert(std::make_pair(
      registration_id,
      PlayerCallbacks(std::move(new_key_cb), std::move(cdm_unset_cb))));
  return registration_id;
}

void PlayerTrackerImpl::UnregisterPlayer(int registration_id) {
  base::AutoLock lock(lock_);
  DCHECK(base::Contains(player_callbacks_map_, registration_id))
      << registration_id;
  player_callbacks_map_.erase(registration_id);
}

void PlayerTrackerImpl::NotifyNewKey() {
  std::vector<base::RepeatingClosure> new_key_callbacks;

  {
    base::AutoLock lock(lock_);
    new_key_callbacks.reserve(player_callbacks_map_.size());
    for (const auto& entry : player_callbacks_map_)
      new_key_callbacks.push_back(entry.second.new_key_cb);
  }

  for (const auto& cb : new_key_callbacks)
    cb.Run();
}

void PlayerTrackerImpl::NotifyCdmUnset() {
  std::vector<base::RepeatingClosure> cdm_unset_callbacks;

  {
    base::AutoLock lock(lock_);
    cdm_unset_callbacks.reserve(player_callbacks_map_.size());
    for (const auto& entry : player_callbacks_map_)
      cdm_unset_callbacks.push_back(entry.second.cdm_unset_cb);
  }

  for (const auto& cb : cdm_unset_callbacks)
    cb.Run();
}

}  // namespace media
