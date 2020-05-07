// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/network_switches.h"

namespace content {

class CommandLineFlagsBrowserTest : public ContentBrowserTest {
 protected:
  network::mojom::NetworkContext* network_context() {
    return content::BrowserContext::GetDefaultStoragePartition(
               shell()->web_contents()->GetBrowserContext())
        ->GetNetworkContext();
  }
};

// Tests that when no special command line flags are passed, requests to port 79
// (finger) fail with ERR_UNSAFE_PORT.
IN_PROC_BROWSER_TEST_F(CommandLineFlagsBrowserTest, Port79DefaultBlocked) {
  EXPECT_EQ(net::ERR_UNSAFE_PORT,
            content::LoadBasicRequest(network_context(),
                                      GURL("http://127.0.0.1:79"), 0, 0, 0));
}

class ExplicitlyAllowPort79BrowserTest : public CommandLineFlagsBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(network::switches::kExplicitlyAllowedPorts,
                                    "79");
  }
};

// Tests that when run with the flag --explicitly-allowed-ports=79, requests to
// port 79 (finger) are permitted.
//
// The request may succeed or fail depending on the platform and what services
// are running, so the test just verifies the reason for failure is not
// ERR_UNSAFE_PORT.
IN_PROC_BROWSER_TEST_F(ExplicitlyAllowPort79BrowserTest, Load) {
  EXPECT_NE(net::ERR_UNSAFE_PORT,
            content::LoadBasicRequest(network_context(),
                                      GURL("http://127.0.0.1:79"), 0, 0, 0));
}

}  // namespace content
