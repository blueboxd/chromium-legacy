// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_lacros.h"

#include "chrome/browser/lacros/lacros_chrome_service_delegate_impl.h"
#include "chromeos/lacros/browser/lacros_chrome_service_impl.h"

ChromeBrowserMainExtraPartsLacros::ChromeBrowserMainExtraPartsLacros() =
    default;

ChromeBrowserMainExtraPartsLacros::~ChromeBrowserMainExtraPartsLacros() =
    default;

void ChromeBrowserMainExtraPartsLacros::PostCreateThreads() {
  lacros_chrome_service_ = std::make_unique<chromeos::LacrosChromeServiceImpl>(
      std::make_unique<LacrosChromeServiceDelegateImpl>());
}
