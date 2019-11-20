// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "build/build_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

// TODO(crbug.com/1026523): Fix flakiness on Win10.
#if defined(OS_WIN)
#define MAYBE_WebLayerBrowserTest DISABLED_WebLayerBrowserTest
#else
#define MAYBE_WebLayerBrowserTest WebLayerBrowserTest
#endif

namespace weblayer {

using MAYBE_WebLayerBrowserTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(MAYBE_WebLayerBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/simple_page.html");

  NavigateAndWaitForCompletion(url, shell());
}

}  // namespace weblayer
