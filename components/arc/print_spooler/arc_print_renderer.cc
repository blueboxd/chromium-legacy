// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/print_spooler/arc_print_renderer.h"

#include <utility>

#include "content/public/browser/web_contents.h"

ArcPrintRenderer::ArcPrintRenderer(content::WebContents* web_contents)
    : binding_(web_contents, this) {}

ArcPrintRenderer::~ArcPrintRenderer() = default;

void ArcPrintRenderer::SetDelegate(
    arc::mojom::PrintRendererDelegatePtr delegate) {
  delegate_ = std::move(delegate);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ArcPrintRenderer)
