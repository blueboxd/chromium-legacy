// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "android_webview/browser/aw_pac_processor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "base/test/task_environment.h"

namespace android_webview {

namespace {
const std::string kScript =
    "function FindProxyForURL(url, host)\n"
    "{\n"
    "\treturn \"PROXY localhost:8080; PROXY localhost:8081; DIRECT \";\n"
    "}\n";

const std::string kScriptDnsResolve =
    "var x = dnsResolveEx(\"localhost\");\n"
    "function FindProxyForURL(url, host) {\n"
    "\treturn \"PROXY \" + x + \":80\";\n"
    "}";

const std::string kRequestUrl = "http://testurl.test";
}  // namespace

class AwPacProcessorTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
           base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AwPacProcessor* pac_processor_ = AwPacProcessor::Get();
};

TEST_F(AwPacProcessorTest, MakeProxyRequest) {
  pac_processor_->SetProxyScript(kScript);
  EXPECT_EQ("PROXY localhost:8080;PROXY localhost:8081;DIRECT",
            pac_processor_->MakeProxyRequest(kRequestUrl));
}

TEST_F(AwPacProcessorTest, MakeProxyRequestDnsResolve) {
  pac_processor_->SetProxyScript(kScriptDnsResolve);
  EXPECT_EQ("PROXY 127.0.0.1:80",
            pac_processor_->MakeProxyRequest(kRequestUrl));
}

}  // namespace android_webview
