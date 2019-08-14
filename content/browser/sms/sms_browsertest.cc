// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"
#include "content/browser/sms/sms_service.h"
#include "content/browser/sms/test/mock_sms_dialog.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/sms/test/mock_sms_web_contents_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace content {

namespace {

class SmsBrowserTest : public ContentBrowserTest {
 public:
  SmsBrowserTest() = default;
  ~SmsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "SmsReceiver");
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    cert_verifier_.SetUpCommandLine(command_line);
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  void SetUpInProcessBrowserTestFixture() override {
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  NiceMock<MockSmsWebContentsDelegate> delegate_;
  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  ContentMockCertVerifier cert_verifier_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Receive) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  shell()->web_contents()->SetDelegate(&delegate_);

  auto* dialog = new NiceMock<MockSmsDialog>();

  SmsDialog::EventHandler hdl;

  EXPECT_CALL(delegate_, CreateSmsDialog(_))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(_, _))
      .WillOnce(
          Invoke([&hdl](RenderFrameHost*, SmsDialog::EventHandler handler) {
            hdl = std::move(handler);
          }));

  EXPECT_CALL(*dialog, SmsReceived()).WillOnce(Invoke([&hdl]() {
    std::move(hdl).Run(SmsDialog::Event::kConfirm);
  }));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  // Test that SMS content can be retrieved after navigator.sms.receive().
  std::string script = R"(
    (async () => {
      let sms = await navigator.sms.receive({timeout: 60});
      return sms.content;
    }) ();
  )";

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&provider, &url]() {
    provider->NotifyReceive(url::Origin::Create(url), "hello");
  }));

  EXPECT_EQ("hello", EvalJs(shell(), script));

  ASSERT_FALSE(provider->HasObservers());
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, AtMostOnePendingSmsRequest) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  shell()->web_contents()->SetDelegate(&delegate_);

  auto* dialog = new NiceMock<MockSmsDialog>();

  SmsDialog::EventHandler hdl;

  EXPECT_CALL(delegate_, CreateSmsDialog(_))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(_, _))
      .WillOnce(
          Invoke([&hdl](RenderFrameHost*, SmsDialog::EventHandler handler) {
            hdl = std::move(handler);
          }));

  EXPECT_CALL(*dialog, SmsReceived()).WillOnce(Invoke([&hdl]() {
    std::move(hdl).Run(SmsDialog::Event::kConfirm);
  }));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  std::string script = R"(
    navigator.sms.receive().then(({content}) => { first = content; });
    navigator.sms.receive().catch(e => e.name);
  )";

  base::RunLoop loop;

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  EXPECT_EQ("AbortError", EvalJs(shell(), script));

  loop.Run();

  provider->NotifyReceive(url::Origin::Create(url), "hello");

  EXPECT_EQ("hello", EvalJs(shell(), "first"));

  ASSERT_FALSE(provider->HasObservers());
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Reload) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  std::string script = R"(
    // kicks off the sms receiver, adding the service
    // to the observer's list.
    navigator.sms.receive({timeout: 60});
    true
  )";

  base::RunLoop loop;

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
    // deliberately avoid calling NotifyReceive() to simulate
    // a request that has been received but not fulfilled.
    loop.Quit();
  }));

  EXPECT_EQ(true, EvalJs(shell(), script));

  loop.Run();

  ASSERT_TRUE(provider->HasObservers());

  // reload the page.
  NavigateToURL(shell(), url);

  ASSERT_FALSE(provider->HasObservers());
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Close) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  std::string script = R"(
    navigator.sms.receive({timeout: 60});
    true
  )";

  base::RunLoop loop;

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  EXPECT_EQ(true, EvalJs(shell(), script));

  loop.Run();

  ASSERT_TRUE(provider->HasObservers());

  shell()->Close();

  ASSERT_FALSE(provider->HasObservers());
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, TwoTabsSameOrigin) {
  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  GURL url = GetTestUrl(nullptr, "simple_page.html");

  NavigateToURL(tab1, url);
  NavigateToURL(tab2, url);

  std::string script = R"(
    navigator.sms.receive({timeout: 60}).then(({content}) => {
      sms = content;
    });
    true
  )";

  SmsDialog::EventHandler hdl_1;
  SmsDialog::EventHandler hdl_2;

  {
    base::RunLoop loop;

    auto* dialog = new NiceMock<MockSmsDialog>();

    tab1->web_contents()->SetDelegate(&delegate_);

    EXPECT_CALL(delegate_, CreateSmsDialog(_))
        .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

    EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
      loop.Quit();
    }));

    EXPECT_CALL(*dialog, Open(_, _))
        .WillOnce(
            Invoke([&hdl_1](RenderFrameHost*, SmsDialog::EventHandler handler) {
              hdl_1 = std::move(handler);
            }));

    // First tab registers an observer.
    EXPECT_EQ(true, EvalJs(tab1, script));

    loop.Run();
  }

  {
    base::RunLoop loop;

    auto* dialog = new NiceMock<MockSmsDialog>();

    tab2->web_contents()->SetDelegate(&delegate_);

    EXPECT_CALL(delegate_, CreateSmsDialog(_))
        .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

    EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
      loop.Quit();
    }));

    EXPECT_CALL(*dialog, Open(_, _))
        .WillOnce(
            Invoke([&hdl_2](RenderFrameHost*, SmsDialog::EventHandler handler) {
              hdl_2 = std::move(handler);
            }));

    // Second tab registers an observer.
    EXPECT_EQ(true, EvalJs(tab2, script));

    loop.Run();
  }

  ASSERT_TRUE(provider->HasObservers());

  provider->NotifyReceive(url::Origin::Create(url), "hello1");

  std::move(hdl_1).Run(SmsDialog::Event::kConfirm);

  EXPECT_EQ("hello1", EvalJs(tab1, "sms"));

  ASSERT_TRUE(provider->HasObservers());

  provider->NotifyReceive(url::Origin::Create(url), "hello2");

  std::move(hdl_2).Run(SmsDialog::Event::kConfirm);

  EXPECT_EQ("hello2", EvalJs(tab2, "sms"));

  ASSERT_FALSE(provider->HasObservers());
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, TwoTabsDifferentOrigin) {
  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL url1 = https_server.GetURL("a.com", "/simple_page.html");
  GURL url2 = https_server.GetURL("b.com", "/simple_page.html");

  NavigateToURL(tab1, url1);
  NavigateToURL(tab2, url2);

  std::string script = R"(
    navigator.sms.receive({timeout: 60}).then(({content}) => {
      sms = content;
    });
    true;
  )";

  base::RunLoop loop;

  EXPECT_CALL(*provider, Retrieve())
      .WillOnce(testing::Return())
      .WillOnce(Invoke([&loop]() { loop.Quit(); }));

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);

  auto* dialog1 = new NiceMock<MockSmsDialog>();
  auto* dialog2 = new NiceMock<MockSmsDialog>();

  SmsDialog::EventHandler hdl_1;
  SmsDialog::EventHandler hdl_2;

  EXPECT_CALL(delegate_, CreateSmsDialog(_))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog1))))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog2))));

  EXPECT_CALL(*dialog1, Open(_, _))
      .WillOnce(
          Invoke([&hdl_1](RenderFrameHost*, SmsDialog::EventHandler handler) {
            hdl_1 = std::move(handler);
          }));

  EXPECT_CALL(*dialog2, Open(_, _))
      .WillOnce(
          Invoke([&hdl_2](RenderFrameHost*, SmsDialog::EventHandler handler) {
            hdl_2 = std::move(handler);
          }));

  EXPECT_EQ(true, EvalJs(tab1, script));
  EXPECT_EQ(true, EvalJs(tab2, script));

  loop.Run();

  ASSERT_TRUE(provider->HasObservers());

  provider->NotifyReceive(url::Origin::Create(url1), "hello1");

  std::move(hdl_1).Run(SmsDialog::Event::kConfirm);

  EXPECT_EQ("hello1", EvalJs(tab1, "sms"));

  ASSERT_TRUE(provider->HasObservers());

  provider->NotifyReceive(url::Origin::Create(url2), "hello2");

  std::move(hdl_2).Run(SmsDialog::Event::kConfirm);

  EXPECT_EQ("hello2", EvalJs(tab2, "sms"));

  ASSERT_FALSE(provider->HasObservers());
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, SmsReceivedAfterTabIsClosed) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  std::string script = R"(
    // kicks off an sms receiver call, but deliberately leaves it hanging.
    navigator.sms.receive({timeout: 60});
    true
  )";

  base::RunLoop loop;

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  EXPECT_EQ(true, EvalJs(shell(), script));

  loop.Run();

  shell()->Close();

  provider->NotifyReceive(url::Origin::Create(url), "hello");
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Cancels) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  StrictMock<MockSmsWebContentsDelegate> delegate;
  shell()->web_contents()->SetDelegate(&delegate);

  auto* dialog = new StrictMock<MockSmsDialog>();

  EXPECT_CALL(delegate, CreateSmsDialog(_))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(_, _))
      .WillOnce(Invoke([](RenderFrameHost*, SmsDialog::EventHandler handler) {
        // Simulates the user pressing "cancel".
        std::move(handler).Run(SmsDialog::Event::kCancel);
      }));

  EXPECT_CALL(*dialog, Close()).WillOnce(Return());

  std::string script = R"(
    (async () => {
      try {
        await navigator.sms.receive({timeout: 10});
        return false;
      } catch (e) {
        // Expects an exception to be thrown.
        return e.name;
      }
    }) ();
  )";

  EXPECT_EQ("AbortError", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, TimesOut) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  StrictMock<MockSmsWebContentsDelegate> delegate;
  shell()->web_contents()->SetDelegate(&delegate);

  auto* dialog = new StrictMock<MockSmsDialog>();

  EXPECT_CALL(delegate, CreateSmsDialog(_))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(_, _))
      .WillOnce(Invoke([](content::RenderFrameHost*,
                          content::SmsDialog::EventHandler event_handler) {
        // Simulates the user pressing "Try again".
        std::move(event_handler).Run(SmsDialog::Event::kTimeout);
      }));

  EXPECT_CALL(*dialog, Close()).WillOnce(Return());

  std::string script = R"(
    (async () => {
      try {
        await navigator.sms.receive({timeout: 1});
        return false;
      } catch (e) {
        // Expects an exception to be thrown.
        return e.name;
      }
    }) ();
  )";

  EXPECT_EQ("TimeoutError", EvalJs(shell(), script));
}

}  // namespace content
