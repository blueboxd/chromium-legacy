// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/content_browser_test.h"
#include "services/test/echo/public/mojom/echo.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ServiceProcessHostBrowserTest = ContentBrowserTest;

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, Launch) {
  mojo::Remote<echo::mojom::EchoService> echo_service;
  auto host = ServiceProcessHost::Launch(&echo_service);

  const std::string kTestString =
      "Aurora borealis! At this time of year? At this time of day? "
      "In this part of the country? Localized entirely within your kitchen?";
  base::RunLoop loop;
  echo_service->EchoString(
      kTestString,
      base::BindLambdaForTesting([&](const std::string& echoed_input) {
        EXPECT_EQ(kTestString, echoed_input);
        loop.Quit();
      }));
  loop.Run();
}

}  // namespace content
