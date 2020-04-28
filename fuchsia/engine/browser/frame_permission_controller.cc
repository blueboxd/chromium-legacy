// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/frame_permission_controller.h"

#include "base/check_op.h"
#include "url/origin.h"

using PermissionState = blink::mojom::PermissionStatus;
using PermissionType = content::PermissionType;

namespace {

size_t GetPermissionIndex(PermissionType type) {
  size_t index = static_cast<size_t>(type);
  DCHECK_LT(index, static_cast<size_t>(PermissionType::NUM));
  return index;
}

}  // namespace

FramePermissionController::PermissionSet::PermissionSet() {
  for (auto& permission : permission_state) {
    permission = PermissionState::DENIED;
  }
}

FramePermissionController::FramePermissionController() = default;
FramePermissionController::~FramePermissionController() = default;

void FramePermissionController::SetPermissionState(PermissionType permission,
                                                   const url::Origin& origin,
                                                   PermissionState state) {
  auto it = per_origin_permissions_.find(origin);
  if (it == per_origin_permissions_.end()) {
    // All permissions are denied by default.
    if (state == PermissionState::DENIED)
      return;

    it = per_origin_permissions_.insert(std::make_pair(origin, PermissionSet()))
             .first;
  }

  it->second.permission_state[GetPermissionIndex(permission)] = state;
}

PermissionState FramePermissionController::GetPermissionState(
    PermissionType permission,
    const url::Origin& origin) {
  auto it = per_origin_permissions_.find(origin);
  if (it == per_origin_permissions_.end()) {
    return PermissionState::DENIED;
  }
  return it->second.permission_state[GetPermissionIndex(permission)];
}

void FramePermissionController::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    const url::Origin& origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<PermissionState>&)> callback) {
  std::vector<PermissionState> result;

  auto it = per_origin_permissions_.find(origin);
  if (it == per_origin_permissions_.end()) {
    result.resize(permissions.size(), PermissionState::DENIED);
  } else {
    result.reserve(permissions.size());
    for (auto& permission : permissions) {
      result.push_back(
          it->second.permission_state[GetPermissionIndex(permission)]);
    }
  }
  std::move(callback).Run(result);
}
