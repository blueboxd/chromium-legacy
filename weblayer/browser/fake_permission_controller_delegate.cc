// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/fake_permission_controller_delegate.h"

#include "base/callback.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"

namespace weblayer {

FakePermissionControllerDelegate::FakePermissionControllerDelegate() = default;

FakePermissionControllerDelegate::~FakePermissionControllerDelegate() = default;

int FakePermissionControllerDelegate::RequestPermission(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  std::move(callback).Run(blink::mojom::PermissionStatus::GRANTED);
  return content::PermissionController::kNoPendingOperation;
}

int FakePermissionControllerDelegate::RequestPermissions(
    const std::vector<content::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  std::vector<blink::mojom::PermissionStatus> result(
      permissions.size(), blink::mojom::PermissionStatus::GRANTED);
  std::move(callback).Run(result);
  return content::PermissionController::kNoPendingOperation;
}

void FakePermissionControllerDelegate::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}

blink::mojom::PermissionStatus
FakePermissionControllerDelegate::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return blink::mojom::PermissionStatus::GRANTED;
}

blink::mojom::PermissionStatus
FakePermissionControllerDelegate::GetPermissionStatusForFrame(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  return GetPermissionStatus(
      permission, requesting_origin,
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetLastCommittedURL()
          .GetOrigin());
}

int FakePermissionControllerDelegate::SubscribePermissionStatusChange(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  return content::PermissionController::kNoPendingOperation;
}

void FakePermissionControllerDelegate::UnsubscribePermissionStatusChange(
    int subscription_id) {}

}  // namespace weblayer
