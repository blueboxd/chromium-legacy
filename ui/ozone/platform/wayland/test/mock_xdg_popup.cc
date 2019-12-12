// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_xdg_popup.h"

#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"

namespace wl {

namespace {

void Grab(struct wl_client* client,
          struct wl_resource* resource,
          struct wl_resource* seat,
          uint32_t serial) {
  GetUserDataAs<MockXdgPopup>(resource)->Grab(serial);
}

}  // namespace

const struct xdg_popup_interface kXdgPopupImpl = {
    &DestroyResource,  // destroy
    &Grab,             // grab
};

const struct zxdg_popup_v6_interface kZxdgPopupV6Impl = {
    &DestroyResource,  // destroy
    &Grab,             // grab
};

MockXdgPopup::MockXdgPopup(wl_resource* resource, wl_resource* surface)
    : ServerObject(resource), surface_(surface) {
  auto* mock_xdg_surface = GetUserDataAs<MockXdgSurface>(surface_);
  if (mock_xdg_surface)
    mock_xdg_surface->set_xdg_popup(nullptr);
}

MockXdgPopup::~MockXdgPopup() {}

}  // namespace wl
