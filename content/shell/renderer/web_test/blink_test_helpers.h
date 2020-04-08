// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_HELPERS_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_HELPERS_H_

#include "third_party/blink/public/platform/web_url.h"

#include <string>

namespace gfx {
class ColorSpace;
}

namespace content {
struct TestPreferences;
struct WebPreferences;

// The TestRunner library keeps its settings in a TestPreferences object.
// The content_shell, however, uses WebPreferences. This method exports the
// settings from the TestRunner library which are relevant for web tests.
void ExportWebTestSpecificPreferences(const TestPreferences& from,
                                      WebPreferences* to);

// Replaces file:///tmp/web_tests/ with the actual path to the
// web_tests directory, or rewrite URLs generated from absolute
// path links in web-platform-tests.
blink::WebURL RewriteWebTestsURL(const std::string& utf8_url, bool is_wpt_mode);

// Get the color space for a given name string. This is not in the ColorSpace
// class to avoid bloating the shipping build.
gfx::ColorSpace GetWebTestColorSpace(const std::string& name);

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_HELPERS_H_
