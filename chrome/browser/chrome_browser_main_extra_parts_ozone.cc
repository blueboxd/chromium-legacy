// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_ozone.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/lifetime/application_lifetime.h"

#if defined(USE_X11)
#include "ui/base/x/x11_error_handler.h"
#else
#include "ui/ozone/public/ozone_platform.h"
#endif

ChromeBrowserMainExtraPartsOzone::ChromeBrowserMainExtraPartsOzone() = default;

ChromeBrowserMainExtraPartsOzone::~ChromeBrowserMainExtraPartsOzone() = default;

void ChromeBrowserMainExtraPartsOzone::PreEarlyInitialization() {
#if defined(USE_X11)
  ui::SetNullErrorHandlers();
#else
  ui::OzonePlatform::PreEarlyInitialization();
#endif
}

void ChromeBrowserMainExtraPartsOzone::PostMainMessageLoopStart() {
  auto shutdown_cb = base::BindOnce(&chrome::SessionEnding);
#if defined(USE_X11)
  ui::SetErrorHandlers(std::move(shutdown_cb));
#else
  ui::OzonePlatform::GetInstance()->PostMainMessageLoopStart(
      std::move(shutdown_cb));
#endif
}

void ChromeBrowserMainExtraPartsOzone::PostMainMessageLoopRun() {
#if defined(USE_X11)
  ui::SetEmptyErrorHandlers();
#else
  ui::OzonePlatform::GetInstance()->PostMainMessageLoopRun();
#endif
}
