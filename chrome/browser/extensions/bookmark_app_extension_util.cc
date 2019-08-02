// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_extension_util.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_MACOSX)
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut_mac.h"
#endif

namespace extensions {

bool CanBookmarkAppRevealAppShim() {
#if defined(OS_MACOSX)
  return true;
#else   // defined(OS_MACOSX)
  return false;
#endif  // !defined(OS_MACOSX)
}

void BookmarkAppRevealAppShim(Profile* profile, const Extension* extension) {
  DCHECK(CanBookmarkAppRevealAppShim());
#if defined(OS_MACOSX)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableHostedAppShimCreation)) {
    Profile* current_profile = profile->GetOriginalProfile();
    web_app::RevealAppShimInFinderForApp(current_profile, extension);
  }
#endif  // defined(OS_MACOSX)
}

}  // namespace extensions
