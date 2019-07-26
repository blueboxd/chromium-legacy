// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"
#include "content/browser/sms/sms_provider.h"
#include "content/browser/sms/sms_service.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/sms_dialog.h"
#include "content/public/browser/web_contents_delegate.h"
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

class MockSmsDialog : public SmsDialog {
 public:
  MockSmsDialog() : SmsDialog() {}
  ~MockSmsDialog() override = default;

  MOCK_METHOD3(Open,
               void(RenderFrameHost*, base::OnceClosure, base::OnceClosure));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(EnableContinueButton, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsDialog);
};

class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD0(CreateSmsDialog, std::unique_ptr<SmsDialog>());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebContentsDelegate);
};

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

  NiceMock<MockWebContentsDelegate> delegate_;
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

  base::OnceClosure on_continue_callback;

  EXPECT_CALL(delegate_, CreateSmsDialog())
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(_, _, _))
      .WillOnce(Invoke([&on_continue_callback](content::RenderFrameHost*,
                                               base::OnceClosure on_continue,
                                               base::OnceClosure on_cancel) {
        on_continue_callback = std::move(on_continue);
      }));

  EXPECT_CALL(*dialog, EnableContinueButton())
      .WillOnce(Invoke([&on_continue_callback]() {
        std::move(on_continue_callback).Run();
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

  base::OnceClosure on_continue_callback;

  EXPECT_CALL(delegate_, CreateSmsDialog())
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(_, _, _))
      .WillOnce(Invoke([&on_continue_callback](content::RenderFrameHost*,
                                               base::OnceClosure on_continue,
                                               base::OnceClosure on_cancel) {
        on_continue_callback = std::move(on_continue);
      }));

  EXPECT_CALL(*dialog, EnableContinueButton())
      .WillOnce(Invoke([&on_continue_callback]() {
        std::move(on_continue_callback).Run();
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

  base::OnceClosure on_continue1, on_continue2;

  {
    base::RunLoop loop;

    auto* dialog = new NiceMock<MockSmsDialog>();

    tab1->web_contents()->SetDelegate(&delegate_);

    EXPECT_CALL(delegate_, CreateSmsDialog())
        .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

    EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
      loop.Quit();
    }));

    EXPECT_CALL(*dialog, Open(_, _, _))
        .WillOnce(Invoke([&on_continue1](content::RenderFrameHost*,
                                         base::OnceClosure on_continue,
                                         base::OnceClosure on_cancel) {
          on_continue1 = std::move(on_continue);
        }));

    // First tab registers an observer.
    EXPECT_EQ(true, EvalJs(tab1, script));

    loop.Run();
  }

  {
    base::RunLoop loop;

    auto* dialog = new NiceMock<MockSmsDialog>();

    tab2->web_contents()->SetDelegate(&delegate_);

    EXPECT_CALL(delegate_, CreateSmsDialog())
        .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

    EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
      loop.Quit();
    }));

    EXPECT_CALL(*dialog, Open(_, _, _))
        .WillOnce(Invoke([&on_continue2](content::RenderFrameHost*,
                                         base::OnceClosure on_continue,
                                         base::OnceClosure on_cancel) {
          on_continue2 = std::move(on_continue);
        }));

    // Second tab registers an observer.
    EXPECT_EQ(true, EvalJs(tab2, script));

    loop.Run();
  }

  ASSERT_TRUE(provider->HasObservers());

  provider->NotifyReceive(url::Origin::Create(url), "hello1");

  std::move(on_continue1).Run();

  EXPECT_EQ("hello1", EvalJs(tab1, "sms"));

  ASSERT_TRUE(provider->HasObservers());

  provider->NotifyReceive(url::Origin::Create(url), "hello2");

  std::move(on_continue2).Run();

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

  base::OnceClosure on_continue_callback1;
  base::OnceClosure on_continue_callback2;

  EXPECT_CALL(delegate_, CreateSmsDialog())
      .WillOnce(Return(ByMove(base::WrapUnique(dialog1))))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog2))));

  EXPECT_CALL(*dialog1, Open(_, _, _))
      .WillOnce(Invoke([&on_continue_callback1](content::RenderFrameHost*,
                                                base::OnceClosure on_continue,
                                                base::OnceClosure on_cancel) {
        on_continue_callback1 = std::move(on_continue);
      }));

  EXPECT_CALL(*dialog2, Open(_, _, _))
      .WillOnce(Invoke([&on_continue_callback2](content::RenderFrameHost*,
                                                base::OnceClosure on_continue,
                                                base::OnceClosure on_cancel) {
        on_continue_callback2 = std::move(on_continue);
      }));

  EXPECT_EQ(true, EvalJs(tab1, script));
  EXPECT_EQ(true, EvalJs(tab2, script));

  loop.Run();

  ASSERT_TRUE(provider->HasObservers());

  provider->NotifyReceive(url::Origin::Create(url1), "hello1");

  std::move(on_continue_callback1).Run();

  EXPECT_EQ("hello1", EvalJs(tab1, "sms"));

  ASSERT_TRUE(provider->HasObservers());

  provider->NotifyReceive(url::Origin::Create(url2), "hello2");

  std::move(on_continue_callback2).Run();

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

  StrictMock<MockWebContentsDelegate> delegate;
  shell()->web_contents()->SetDelegate(&delegate);

  auto* dialog = new StrictMock<MockSmsDialog>();

  EXPECT_CALL(delegate, CreateSmsDialog())
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(_, _, _))
      .WillOnce(
          Invoke([](content::RenderFrameHost*, base::OnceClosure on_continue,
                    base::OnceClosure on_cancel) {
            // Simulates the user pressing "cancel".
            std::move(on_cancel).Run();
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
