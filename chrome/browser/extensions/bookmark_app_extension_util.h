// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_EXTENSION_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_EXTENSION_UTIL_H_

#include "base/callback_forward.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

bool CanBookmarkAppReparentTab(Profile* profile,
                               const Extension* extension,
                               bool shortcut_created);
void BookmarkAppReparentTab(content::WebContents* contents,
                            const Extension* extension);

bool CanBookmarkAppRevealAppShim();
void BookmarkAppRevealAppShim(Profile* profile, const Extension* extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_EXTENSION_UTIL_H_
