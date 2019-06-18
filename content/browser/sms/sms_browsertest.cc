// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_provider.h"
#include "content/browser/sms/sms_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace content {

namespace {

class MockSmsProvider : public SmsProvider {
 public:
  MockSmsProvider() = default;
  ~MockSmsProvider() override = default;

  MOCK_METHOD0(Retrieve, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsProvider);
};

class SmsBrowserTest : public ContentBrowserTest {
 public:
  SmsBrowserTest() = default;
  ~SmsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "SmsReceiver");
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Receive) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  auto* mock = new NiceMock<MockSmsProvider>();

  auto* sms_service = static_cast<SmsServiceImpl*>(
      shell()->web_contents()->GetBrowserContext()->GetSmsService());
  sms_service->SetSmsProviderForTest(base::WrapUnique(mock));

  // Test that SMS content can be retrieved after navigator.sms.receive().
  std::string script = R"(
    (async () => {
      let sms = await navigator.sms.receive({timeout: 60});
      return sms.content;
    }) ();
  )";

  EXPECT_CALL(*mock, Retrieve()).WillOnce(Invoke([&mock, &url]() {
    mock->NotifyReceive(url::Origin::Create(url), "hello");
  }));

  EXPECT_EQ("hello", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, ReceiveMultiple) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  auto* mock = new NiceMock<MockSmsProvider>();

  auto* sms_service = static_cast<SmsServiceImpl*>(
      shell()->web_contents()->GetBrowserContext()->GetSmsService());
  sms_service->SetSmsProviderForTest(base::WrapUnique(mock));

  // Test that SMS content can retrieve multiple messages.
  std::string script = R"(
    (async () => {
      let sms1 = navigator.sms.receive();
      let sms2 = navigator.sms.receive();

      let msg1 = await sms1;
      let msg2 = await sms2;

      return [msg1.content, msg2.content];
    }) ();
  )";

  EXPECT_CALL(*mock, Retrieve())
      .WillOnce(Invoke([&mock, &url]() {
        mock->NotifyReceive(url::Origin::Create(url), "hello1");
      }))
      .WillOnce(Invoke([&mock, &url]() {
        mock->NotifyReceive(url::Origin::Create(url), "hello2");
      }));

  base::ListValue result = EvalJs(shell(), script).ExtractList();
  ASSERT_EQ(2u, result.GetList().size());
  EXPECT_EQ("hello1", result.GetList()[0].GetString());
  EXPECT_EQ("hello2", result.GetList()[1].GetString());
}

}  // namespace content
