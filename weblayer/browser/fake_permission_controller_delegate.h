// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FAKE_PERMISSION_CONTROLLER_DELEGATE_H_
#define WEBLAYER_BROWSER_FAKE_PERMISSION_CONTROLLER_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/public/browser/permission_controller_delegate.h"

namespace weblayer {

// Temporary permission controller delegate which grants all permissions. Once
// permissions have been implemented, this will be removed. This is only used if
// the --weblayer-fake-permissions switch is passed on the command line.
class FakePermissionControllerDelegate
    : public content::PermissionControllerDelegate {
 public:
  FakePermissionControllerDelegate();
  ~FakePermissionControllerDelegate() override;

  // PermissionControllerDelegate implementation.
  int RequestPermission(content::PermissionType permission,
                        content::RenderFrameHost* render_frame_host,
                        const GURL& requesting_origin,
                        bool user_gesture,
                        base::OnceCallback<void(blink::mojom::PermissionStatus)>
                            callback) override;
  int RequestPermissions(
      const std::vector<content::PermissionType>& permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) override;
  int SubscribePermissionStatusChange(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(int subscription_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePermissionControllerDelegate);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FAKE_PERMISSION_CONTROLLER_DELEGATE_H_
