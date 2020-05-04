// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_LAYOUT_DUMP_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_LAYOUT_DUMP_H_

#include <string>

#include "content/shell/renderer/web_test/web_test_runtime_flags.h"

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace content {

// Dumps textual representation of |frame| contents.  Exact dump mode depends
// on |flags| (i.e. dump_as_text VS dump_as_markup and/or is_printing).
std::string DumpLayoutAsString(blink::WebLocalFrame* frame,
                               const WebTestRuntimeFlags& flags);

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_LAYOUT_DUMP_H_
