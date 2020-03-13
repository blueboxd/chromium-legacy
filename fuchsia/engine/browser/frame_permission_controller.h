// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_FRAME_PERMISSION_CONTROLLER_H_
#define FUCHSIA_ENGINE_BROWSER_FRAME_PERMISSION_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "content/public/browser/permission_type.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace url {
class Origin;
}

// FramePermissionController is responsible for web permissions state for a
// fuchsia.web.Frame instance.
class FramePermissionController {
 public:
  FramePermissionController();
  ~FramePermissionController();

  FramePermissionController(FramePermissionController&) = delete;
  FramePermissionController& operator=(FramePermissionController&) = delete;

  // Sets the |state| for the specified |permission| and |origin|.
  void SetPermissionState(content::PermissionType permission,
                          const url::Origin& origin,
                          blink::mojom::PermissionStatus state);

  // Returns current permission state of the specified |permission| and
  // |origin|.
  blink::mojom::PermissionStatus GetPermissionState(
      content::PermissionType permission,
      const url::Origin& origin);

  // Requests permission state for the specified |permissions|. When the request
  // is resolved, the |callback| is called with a list of status values, one for
  // each value in |permissions|, in the same order.
  //
  // TODO(crbug.com/922833): Current implementation doesn't actually prompt the
  // user: all permissions in the PROMPT state are denied silently. Define
  // fuchsia.web.PermissionManager protocol and use it to request permissions.
  void RequestPermissions(
      const std::vector<content::PermissionType>& permissions,
      const url::Origin& origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback);

 private:
  struct PermissionSet {
    PermissionSet();

    blink::mojom::PermissionStatus
        permission_state[static_cast<int>(content::PermissionType::NUM)];
  };

  std::map<url::Origin, PermissionSet> per_origin_permissions_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_FRAME_PERMISSION_CONTROLLER_H_