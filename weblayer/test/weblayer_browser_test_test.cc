// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "build/build_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

// Disabled on Windows, see crbug.com/1034764
#if !defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(WebLayerBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/simple_page.html");

  NavigateAndWaitForCompletion(url, shell());
}
#endif  // !defined(OS_WIN)

}  // namespace weblayer
