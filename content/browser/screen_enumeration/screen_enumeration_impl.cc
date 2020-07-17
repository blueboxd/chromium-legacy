// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_enumeration/screen_enumeration_impl.h"

#include <utility>
#include <vector>

#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace content {

ScreenEnumerationImpl::ScreenEnumerationImpl(RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

ScreenEnumerationImpl::~ScreenEnumerationImpl() = default;

void ScreenEnumerationImpl::Bind(
    mojo::PendingReceiver<blink::mojom::ScreenEnumeration> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ScreenEnumerationImpl::GetDisplays(GetDisplaysCallback callback) {
  // Ensure the callback is run if this object is prematurely destroyed.
  auto scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), nullptr);

  auto* permission_controller = PermissionControllerImpl::FromBrowserContext(
      render_frame_host_->GetProcess()->GetBrowserContext());
  permission_controller->RequestPermission(
      PermissionType::WINDOW_PLACEMENT, render_frame_host_,
      render_frame_host_->GetLastCommittedOrigin().GetURL(),
      /*user_gesture=*/false,
      base::BindOnce(&ScreenEnumerationImpl::GetDisplaysWithPermissionStatus,
                     weak_factory_.GetWeakPtr(), std::move(scoped_callback)));
}

void ScreenEnumerationImpl::GetDisplaysWithPermissionStatus(
    GetDisplaysCallback callback,
    blink::mojom::PermissionStatus permission_status) {
  if (permission_status != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(nullptr);
    return;
  }

  display::Screen* screen = display::Screen::GetScreen();
  auto result = blink::mojom::Displays::New();
  result->displays = screen->GetAllDisplays();
  result->internal_id = display::Display::HasInternalDisplay()
                            ? display::Display::InternalDisplayId()
                            : display::kInvalidDisplayId;
  result->primary_id = screen->GetPrimaryDisplay().id();
  std::move(callback).Run(std::move(result));
}

void ScreenEnumerationImpl::HasMultipleDisplays(
    HasMultipleDisplaysCallback callback) {
  auto result = display::Screen::GetScreen()->GetNumDisplays() > 1
                    ? blink::mojom::MultipleDisplays::kTrue
                    : blink::mojom::MultipleDisplays::kFalse;
  std::move(callback).Run(result);
}

}  // namespace content