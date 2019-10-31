// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_constants.h"

#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace web_app {

static_assert(Source::kMinValue == 0, "Source enum should be zero based");

bool IsSuccess(InstallResultCode code) {
  return code == InstallResultCode::kSuccessNewInstall ||
         code == InstallResultCode::kSuccessAlreadyInstalled;
}

blink::mojom::DisplayMode ResolveEffectiveDisplayMode(
    blink::mojom::DisplayMode app_display_mode,
    blink::mojom::DisplayMode user_display_mode) {
  switch (user_display_mode) {
    case blink::mojom::DisplayMode::kBrowser:
      return blink::mojom::DisplayMode::kBrowser;
    case blink::mojom::DisplayMode::kUndefined:
    case blink::mojom::DisplayMode::kMinimalUi:
    case blink::mojom::DisplayMode::kFullscreen:
      NOTREACHED();
      FALLTHROUGH;
    case blink::mojom::DisplayMode::kStandalone:
      break;
  }

  switch (app_display_mode) {
    case blink::mojom::DisplayMode::kBrowser:
    case blink::mojom::DisplayMode::kMinimalUi:
      return blink::mojom::DisplayMode::kMinimalUi;
    case blink::mojom::DisplayMode::kUndefined:
      NOTREACHED();
      FALLTHROUGH;
    case blink::mojom::DisplayMode::kStandalone:
    case blink::mojom::DisplayMode::kFullscreen:
      return blink::mojom::DisplayMode::kStandalone;
  }
}

}  // namespace web_app
