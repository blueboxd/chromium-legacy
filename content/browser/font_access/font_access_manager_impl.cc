// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager_impl.h"

#include "base/bind.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace content {

FontAccessManagerImpl::FontAccessManagerImpl() = default;

FontAccessManagerImpl::~FontAccessManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FontAccessManagerImpl::BindReceiver(
    const BindingContext& context,
    mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver), context);
}

void FontAccessManagerImpl::RequestPermission(
    RequestPermissionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const BindingContext& context = receivers_.current_context();
  RenderFrameHost* rfh = RenderFrameHost::FromID(context.frame_id);

  auto* permission_controller = PermissionControllerImpl::FromBrowserContext(
      rfh->GetProcess()->GetBrowserContext());
  permission_controller->RequestPermission(
      PermissionType::FONT_ACCESS, rfh, context.origin.GetURL(),
      /*user_gesture=*/false,
      base::BindOnce(
          [](RequestPermissionCallback callback,
             blink::mojom::PermissionStatus status) {
            std::move(callback).Run(status);
          },
          std::move(callback)));
}

}  // namespace content
