// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view.h"

namespace chromecast {

std::unique_ptr<content::BluetoothChooser>
CastWebView::Delegate::RunBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
  return nullptr;
}

CastWebView::CreateParams::CreateParams() = default;

CastWebView::CreateParams::CreateParams(const CreateParams& other) = default;

CastWebView::CreateParams::~CreateParams() = default;

}  // namespace chromecast
