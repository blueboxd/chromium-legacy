// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_data_endpoint.h"

#include "url/origin.h"

namespace ui {

ClipboardDataEndpoint::ClipboardDataEndpoint(const GURL& url) : url_(url) {}

ClipboardDataEndpoint::ClipboardDataEndpoint(
    const ClipboardDataEndpoint& other) = default;

ClipboardDataEndpoint::ClipboardDataEndpoint(ClipboardDataEndpoint&& other) =
    default;

ClipboardDataEndpoint& ClipboardDataEndpoint::operator=(
    const ClipboardDataEndpoint& other) = default;

ClipboardDataEndpoint& ClipboardDataEndpoint::operator=(
    ClipboardDataEndpoint&& other) = default;

ClipboardDataEndpoint::~ClipboardDataEndpoint() = default;

}  // namespace ui
