// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_enumeration/screen_enumeration_impl.h"

#include <memory>

#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/origin.h"

namespace content {

// static
void ScreenEnumerationImpl::Create(
    mojo::PendingReceiver<blink::mojom::ScreenEnumeration> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<ScreenEnumerationImpl>(),
                              std::move(receiver));
}

ScreenEnumerationImpl::ScreenEnumerationImpl() = default;
ScreenEnumerationImpl::~ScreenEnumerationImpl() = default;

void ScreenEnumerationImpl::RequestDisplays(RequestDisplaysCallback callback) {
  display::Screen* screen = display::Screen::GetScreen();
  std::vector<display::Display> displays = screen->GetAllDisplays();
  int64_t primary_id = screen->GetPrimaryDisplay().id();

  std::vector<blink::mojom::DisplayPtr> result;
  for (const display::Display& display : displays) {
    const gfx::Rect& bounds = display.bounds();
    auto mojo_display = blink::mojom::Display::New();
    // TODO(staphany): Get actual display name instead of hardcoded value.
    mojo_display->name = "Generic Display";
    mojo_display->scale_factor = display.device_scale_factor();
    mojo_display->width = bounds.width();
    mojo_display->height = bounds.height();
    mojo_display->left = bounds.x();
    mojo_display->top = bounds.y();
    mojo_display->color_depth = display.color_depth();
    mojo_display->is_primary = (display.id() == primary_id);
    mojo_display->is_internal = display.IsInternal();
    result.emplace_back(std::move(mojo_display));
  }
  // TODO(staphany): Add a permission prompt, and return empty |result| and
  // |false| if the permission check fails.
  std::move(callback).Run(std::move(result), true);
}

}  // namespace content