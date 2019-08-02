// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_EXTENSION_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_EXTENSION_UTIL_H_

class Profile;

namespace extensions {

class Extension;

bool CanBookmarkAppRevealAppShim();
void BookmarkAppRevealAppShim(Profile* profile, const Extension* extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_EXTENSION_UTIL_H_
