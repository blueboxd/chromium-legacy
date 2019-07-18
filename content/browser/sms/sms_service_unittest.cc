// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "content/browser/sms/sms_provider.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_service_manager_context.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"

using base::BindLambdaForTesting;
using base::Optional;
using base::TimeDelta;
using blink::mojom::SmsReceiver;
using blink::mojom::SmsReceiverPtr;
using blink::mojom::SmsStatus;
using std::string;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using url::Origin;

namespace content {

class RenderFrameHost;

using blink::mojom::SmsReceiverPtr;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace {

const char kTestUrl[] = "https://www.google.com";

class MockSmsProvider : public SmsProvider {
 public:
  MockSmsProvider() = default;
  ~MockSmsProvider() override = default;

  MOCK_METHOD0(Retrieve, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsProvider);
};

class MockSmsDialog : public SmsDialog {
 public:
  MockSmsDialog() : SmsDialog() {}
  ~MockSmsDialog() override = default;

  MOCK_METHOD2(Open, void(RenderFrameHost*, base::OnceCallback<void()>));
  MOCK_METHOD0(Close, void());

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

// Service encapsulates a SmsService endpoint, with all of its dependencies
// mocked out (and the common plumbing needed to inject them), and a
// SmsReceiverPtr endpoint that tests can use to make requests.
// It exposes some common methods, like MakeRequest and NotifyReceive, but it
// also exposes the low level mocks that enables tests to set expectations and
// control the testing environment.
class Service {
 public:
  Service(WebContents* web_contents, const Origin& origin) {
    WebContentsImpl* web_contents_impl =
        reinterpret_cast<WebContentsImpl*>(web_contents);
    web_contents_impl->SetDelegate(&delegate_);
    service_ = std::make_unique<SmsService>(&provider_, origin,
                                            web_contents->GetMainFrame(),
                                            mojo::MakeRequest(&service_ptr_));
  }

  Service(WebContents* web_contents)
      : Service(web_contents,
                web_contents->GetMainFrame()->GetLastCommittedOrigin()) {}

  NiceMock<MockWebContentsDelegate>* delegate() { return &delegate_; }
  NiceMock<MockSmsProvider>* provider() { return &provider_; }
  blink::mojom::SmsReceiverPtr* client() { return &service_ptr_; }

  void SetSmsService(std::unique_ptr<SmsService> service) {
    service_ = std::move(service);
  }

  void MakeRequest(TimeDelta timeout, SmsReceiver::ReceiveCallback callback) {
    service_ptr_->Receive(timeout, std::move(callback));
  }

  void NotifyReceive(const GURL& url, const string& message) {
    provider_.NotifyReceive(Origin::Create(url), message);
  }

 private:
  NiceMock<MockWebContentsDelegate> delegate_;
  NiceMock<MockSmsProvider> provider_;
  blink::mojom::SmsReceiverPtr service_ptr_;
  std::unique_ptr<SmsService> service_;
};

class SmsServiceTest : public RenderViewHostTestHarness {
 protected:
  SmsServiceTest() {}
  ~SmsServiceTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SmsServiceTest);
};

}  // namespace

TEST_F(SmsServiceTest, Basic) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&](SmsStatus status, const Optional<string>& sms) {
        EXPECT_EQ("hi", sms.value());
        EXPECT_EQ(SmsStatus::kSuccess, status);
        loop.Quit();
      }));

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  loop.Run();

  ASSERT_FALSE(service.provider()->HasObservers());
}

TEST_F(SmsServiceTest, HandlesMultipleCalls) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  {
    base::RunLoop loop;

    service.MakeRequest(TimeDelta::FromSeconds(10),
                        BindLambdaForTesting(
                            [&](SmsStatus status, const Optional<string>& sms) {
                              EXPECT_EQ("first", sms.value());
                              EXPECT_EQ(SmsStatus::kSuccess, status);
                              loop.Quit();
                            }));

    EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "first");
    }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    service.MakeRequest(TimeDelta::FromSeconds(10),
                        BindLambdaForTesting(
                            [&](SmsStatus status, const Optional<string>& sms) {
                              EXPECT_EQ("second", sms.value());
                              EXPECT_EQ(SmsStatus::kSuccess, status);
                              loop.Quit();
                            }));

    EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "second");
    }));

    loop.Run();
  }
}

TEST_F(SmsServiceTest, IgnoreFromOtherOrigins) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status;
  Optional<string> response;

  base::RunLoop listen_loop, sms_loop;

  EXPECT_CALL(*service.provider(), Retrieve())
      .WillOnce(Invoke([&listen_loop]() { listen_loop.Quit(); }));

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&](SmsStatus status, const Optional<string>& sms) {
        sms_status = status;
        response = sms;
        sms_loop.Quit();
      }));

  listen_loop.Run();

  // Delivers an SMS from an unrelated origin first and expect the receiver to
  // ignore it.
  service.NotifyReceive(GURL("http://b.com"), "wrong");
  service.NotifyReceive(GURL(kTestUrl), "right");

  sms_loop.Run();

  EXPECT_EQ("right", response.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status);
}

TEST_F(SmsServiceTest, ExpectOneReceiveTwo) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status;
  Optional<string> response;

  base::RunLoop listen_loop, sms_loop;

  EXPECT_CALL(*service.provider(), Retrieve())
      .WillOnce(Invoke([&listen_loop]() { listen_loop.Quit(); }));

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&](SmsStatus status, const Optional<string>& sms) {
        sms_status = status;
        response = sms;
        sms_loop.Quit();
      }));

  listen_loop.Run();

  // Delivers two SMSes for the same origin, even if only one was being
  // expected.
  service.NotifyReceive(GURL(kTestUrl), "first");
  service.NotifyReceive(GURL(kTestUrl), "second");

  sms_loop.Run();

  EXPECT_EQ("first", response.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status);
}

TEST_F(SmsServiceTest, ExpectTwoReceiveTwoConcurrently) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status1;
  Optional<string> response1;
  SmsStatus sms_status2;
  Optional<string> response2;

  base::RunLoop listen_loop, sms1_loop, sms2_loop;

  // Expects two Receive() calls to be made before any of them gets
  // an SMS to resolve them.
  EXPECT_CALL(*service.provider(), Retrieve())
      .WillOnce(testing::Return())
      .WillOnce(Invoke([&listen_loop]() { listen_loop.Quit(); }));

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&](SmsStatus status, const Optional<string>& sms) {
        sms_status1 = status;
        response1 = sms;
        sms1_loop.Quit();
      }));

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&](SmsStatus status, const Optional<string>& sms) {
        sms_status2 = status;
        response2 = sms;
        sms2_loop.Quit();
      }));

  listen_loop.Run();

  // Delivers the first SMS.

  service.NotifyReceive(GURL(kTestUrl), "first");

  sms1_loop.Run();

  EXPECT_EQ("first", response1.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status1);

  // Delivers the second SMS.

  service.NotifyReceive(GURL(kTestUrl), "second");

  sms2_loop.Run();

  EXPECT_EQ("second", response2.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status2);
}

TEST_F(SmsServiceTest, Timeout) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.MakeRequest(
      TimeDelta::FromSeconds(0),
      BindLambdaForTesting([&](SmsStatus status, const Optional<string>& sms) {
        EXPECT_EQ(SmsStatus::kTimeout, status);
        loop.Quit();
      }));

  loop.Run();
}

TEST_F(SmsServiceTest, CleansUp) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      reinterpret_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  blink::mojom::SmsReceiverPtr service_ptr;
  SmsService::Create(&provider, main_rfh(), mojo::MakeRequest(&service_ptr));

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve()).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service_ptr->Receive(base::TimeDelta::FromSeconds(10),
                       base::BindLambdaForTesting(
                           [&reload](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
                             EXPECT_EQ(blink::mojom::SmsStatus::kTimeout,
                                       status);
                             reload.Quit();
                           }));

  navigate.Run();

  // Simulates the user reloading the page and navigating away, which
  // destructs the service.
  NavigateAndCommit(GURL(kTestUrl));

  reload.Run();

  ASSERT_FALSE(provider.HasObservers());
}

TEST_F(SmsServiceTest, PromptsDialog) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  auto* dialog = new NiceMock<MockSmsDialog>();

  EXPECT_CALL(*service.delegate(), CreateSmsDialog())
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  base::RunLoop loop;

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  EXPECT_CALL(*dialog, Open(main_rfh(), _)).WillOnce(Return());

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&](SmsStatus status, const Optional<string>& sms) {
        EXPECT_EQ("hi", sms.value());
        EXPECT_EQ(SmsStatus::kSuccess, status);
        loop.Quit();
      }));

  EXPECT_CALL(*dialog, Close()).WillOnce(Return());

  loop.Run();

  ASSERT_FALSE(service.provider()->HasObservers());
}

TEST_F(SmsServiceTest, Cancel) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&](SmsStatus status, const Optional<string>& sms) {
        EXPECT_EQ(SmsStatus::kCancelled, status);
        loop.Quit();
      }));

  auto* dialog = new NiceMock<MockSmsDialog>();

  EXPECT_CALL(*service.delegate(), CreateSmsDialog())
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(main_rfh(), _))
      .WillOnce(
          Invoke([](RenderFrameHost*, base::OnceCallback<void()> on_cancel) {
            // Simulates the user pressing "cancel".
            std::move(on_cancel).Run();
          }));

  loop.Run();

  ASSERT_FALSE(service.provider()->HasObservers());
}

TEST_F(SmsServiceTest, TimeoutClosesDialog) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.MakeRequest(
      TimeDelta::FromSeconds(0),
      BindLambdaForTesting([&](SmsStatus status, const Optional<string>& sms) {
        EXPECT_EQ(SmsStatus::kTimeout, status);
        loop.Quit();
      }));

  auto* dialog = new NiceMock<MockSmsDialog>();

  EXPECT_CALL(*service.delegate(), CreateSmsDialog())
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Return());

  // Deliberately avoid calling the on_cancel callback, to simulate the
  // sms being timed out before the user cancels it.
  EXPECT_CALL(*dialog, Open(main_rfh(), _)).WillOnce(Return());

  EXPECT_CALL(*dialog, Close()).WillOnce(Return());

  loop.Run();
}

TEST_F(SmsServiceTest, SecondRequestTimesOutEarlierThanFirstRequest) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop sms_loop1, sms_loop2;

  service.MakeRequest(TimeDelta::FromSeconds(10),
                      BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                               const Optional<string>& sms) {
                        EXPECT_EQ(SmsStatus::kSuccess, status);
                        EXPECT_EQ("first", sms.value());
                        sms_loop1.Quit();
                      }));

  service.MakeRequest(
      TimeDelta::FromSeconds(0),
      BindLambdaForTesting([&](SmsStatus status, const Optional<string>& sms) {
        EXPECT_EQ(SmsStatus::kTimeout, status);
        sms_loop2.Quit();
      }));

  // The second request immediately times out because it uses TimeDelta of 0
  // seconds.
  sms_loop2.Run();

  // Delivers the first SMS.

  service.NotifyReceive(GURL(kTestUrl), "first");

  sms_loop1.Run();
}

}  // namespace content
