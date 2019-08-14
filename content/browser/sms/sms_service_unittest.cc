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
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/sms/test/mock_sms_dialog.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/sms/test/mock_sms_web_contents_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
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

namespace {

const char kTestUrl[] = "https://www.google.com";

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

  NiceMock<MockSmsWebContentsDelegate>* delegate() { return &delegate_; }
  NiceMock<MockSmsProvider>* provider() { return &provider_; }

  void SetupSmsDialog(RenderFrameHost* rfh) {
    auto* dialog = new NiceMock<MockSmsDialog>();

    EXPECT_CALL(*delegate(), CreateSmsDialog(_))
        .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

    EXPECT_CALL(*dialog, Open(rfh, _))
        .WillOnce(
            Invoke([&](RenderFrameHost*, SmsDialog::EventHandler handler) {
              handler_ = std::move(handler);
            }));

    EXPECT_CALL(*dialog, SmsReceived()).WillOnce(Invoke([&]() {
      // Simulates user clicking the "Confirm" button to verify received
      // sms.
      std::move(handler_).Run(SmsDialog::Event::kConfirm);
    }));

    EXPECT_CALL(*dialog, Close()).WillOnce(Return());
  }

  void MakeRequest(TimeDelta timeout, SmsReceiver::ReceiveCallback callback) {
    service_ptr_->Receive(timeout, std::move(callback));
  }

  void NotifyReceive(const GURL& url, const string& message) {
    provider_.NotifyReceive(Origin::Create(url), message);
  }

 private:
  NiceMock<MockSmsWebContentsDelegate> delegate_;
  NiceMock<MockSmsProvider> provider_;
  blink::mojom::SmsReceiverPtr service_ptr_;
  std::unique_ptr<SmsService> service_;
  SmsDialog::EventHandler handler_;
};

class SmsServiceTest : public RenderViewHostTestHarness {
 protected:
  SmsServiceTest() {}
  ~SmsServiceTest() override {}

  std::unique_ptr<base::HistogramSamples> GetHistogramSamplesSinceTestStart(
      const std::string& name) {
    return histogram_tester_.GetHistogramSamplesSinceCreation(name);
  }

 private:
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(SmsServiceTest);
};

}  // namespace

TEST_F(SmsServiceTest, Basic) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.SetupSmsDialog(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            EXPECT_EQ("hi", sms.value());
            EXPECT_EQ(SmsStatus::kSuccess, status);
            loop.Quit();
          }));

  loop.Run();

  ASSERT_FALSE(service.provider()->HasObservers());
}

TEST_F(SmsServiceTest, HandlesMultipleCalls) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  {
    base::RunLoop loop;

    service.SetupSmsDialog(main_rfh());

    EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "first");
    }));

    service.MakeRequest(
        TimeDelta::FromSeconds(10),
        BindLambdaForTesting(
            [&loop](SmsStatus status, const Optional<string>& sms) {
              EXPECT_EQ("first", sms.value());
              EXPECT_EQ(SmsStatus::kSuccess, status);
              loop.Quit();
            }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    service.SetupSmsDialog(main_rfh());

    EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "second");
    }));

    service.MakeRequest(
        TimeDelta::FromSeconds(10),
        BindLambdaForTesting(
            [&loop](SmsStatus status, const Optional<string>& sms) {
              EXPECT_EQ("second", sms.value());
              EXPECT_EQ(SmsStatus::kSuccess, status);
              loop.Quit();
            }));

    loop.Run();
  }
}

TEST_F(SmsServiceTest, IgnoreFromOtherOrigins) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status;
  Optional<string> response;

  base::RunLoop sms_loop;

  service.SetupSmsDialog(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    // Delivers an SMS from an unrelated origin first and expect the
    // receiver to ignore it.
    service.NotifyReceive(GURL("http://b.com"), "wrong");
    service.NotifyReceive(GURL(kTestUrl), "right");
  }));

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&sms_status, &response, &sms_loop](
                               SmsStatus status, const Optional<string>& sms) {
        sms_status = status;
        response = sms;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ("right", response.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status);
}

TEST_F(SmsServiceTest, ExpectOneReceiveTwo) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status;
  Optional<string> response;

  base::RunLoop sms_loop;

  service.SetupSmsDialog(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    // Delivers two SMSes for the same origin, even if only one was being
    // expected.
    ASSERT_TRUE(service.provider()->HasObservers());
    service.NotifyReceive(GURL(kTestUrl), "first");
    ASSERT_FALSE(service.provider()->HasObservers());
    service.NotifyReceive(GURL(kTestUrl), "second");
  }));

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&sms_status, &response, &sms_loop](
                               SmsStatus status, const Optional<string>& sms) {
        sms_status = status;
        response = sms;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ("first", response.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status);
}

TEST_F(SmsServiceTest, AtMostOnePendingSmsRequest) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status1;
  Optional<string> response1;
  SmsStatus sms_status2;
  Optional<string> response2;

  base::RunLoop sms1_loop, sms2_loop;

  // Expects only one CreateSmsDialog() and Open() call to be made since at most
  // one sms request can be pending.
  service.SetupSmsDialog(main_rfh());

  // Expects only one Retrieve() call to be made since at most one sms request
  // can be pending.
  EXPECT_CALL(*service.provider(), Retrieve()).Times(1);

  // Make the first SMS request.
  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&sms_status1, &response1, &sms1_loop](
                               SmsStatus status, const Optional<string>& sms) {
        sms_status1 = status;
        response1 = sms;
        sms1_loop.Quit();
      }));

  // Make the second SMS request, and it will be canceled because the first SMS
  // request is still pending.
  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting([&sms_status2, &response2, &sms2_loop](
                               SmsStatus status, const Optional<string>& sms) {
        sms_status2 = status;
        response2 = sms;
        sms2_loop.Quit();
      }));

  sms2_loop.Run();

  EXPECT_EQ(base::nullopt, response2);
  EXPECT_EQ(SmsStatus::kCancelled, sms_status2);

  // Delivers the first SMS.
  service.NotifyReceive(GURL(kTestUrl), "first");

  sms1_loop.Run();

  EXPECT_EQ("first", response1.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status1);
}

TEST_F(SmsServiceTest, Timeout) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.MakeRequest(
      TimeDelta::FromSeconds(0),
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            EXPECT_EQ(SmsStatus::kTimeout, status);
            EXPECT_EQ(base::nullopt, sms);
            loop.Quit();
          }));

  auto* dialog = new NiceMock<MockSmsDialog>();

  EXPECT_CALL(*service.delegate(), CreateSmsDialog(_))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(main_rfh(), _))
      .WillOnce(Invoke([](content::RenderFrameHost*,
                          content::SmsDialog::EventHandler event_handler) {
        // Simulates the user pressing "Try again".
        std::move(event_handler).Run(SmsDialog::Event::kTimeout);
      }));

  loop.Run();
}

TEST_F(SmsServiceTest, CleansUp) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsWebContentsDelegate> delegate;
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

  service_ptr->Receive(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting(
          [&reload](SmsStatus status, const base::Optional<std::string>& sms) {
            EXPECT_EQ(SmsStatus::kTimeout, status);
            EXPECT_EQ(base::nullopt, sms);
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

  base::RunLoop loop;

  service.SetupSmsDialog(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            EXPECT_EQ("hi", sms.value());
            EXPECT_EQ(SmsStatus::kSuccess, status);
            loop.Quit();
          }));

  loop.Run();

  ASSERT_FALSE(service.provider()->HasObservers());
}

TEST_F(SmsServiceTest, Cancel) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            EXPECT_EQ(SmsStatus::kCancelled, status);
            EXPECT_EQ(base::nullopt, sms);
            loop.Quit();
          }));

  auto* dialog = new NiceMock<MockSmsDialog>();

  EXPECT_CALL(*service.delegate(), CreateSmsDialog(_))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(main_rfh(), _))
      .WillOnce(Invoke([](RenderFrameHost*, SmsDialog::EventHandler handler) {
        // Simulates the user pressing "Cancel".
        std::move(handler).Run(SmsDialog::Event::kCancel);
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
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            EXPECT_EQ(SmsStatus::kTimeout, status);
            EXPECT_EQ(base::nullopt, sms);
            loop.Quit();
          }));

  auto* dialog = new NiceMock<MockSmsDialog>();

  EXPECT_CALL(*service.delegate(), CreateSmsDialog(_))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Return());

  // Deliberately avoid calling the on_cancel callback, to simulate the
  // sms being timed out before the user cancels it.
  EXPECT_CALL(*dialog, Open(main_rfh(), _))
      .WillOnce(Invoke([](content::RenderFrameHost*,
                          content::SmsDialog::EventHandler event_handler) {
        // Simulates the user pressing "Try again".
        std::move(event_handler).Run(SmsDialog::Event::kTimeout);
      }));

  EXPECT_CALL(*dialog, Close()).WillOnce(Return());

  loop.Run();
}

TEST_F(SmsServiceTest, SecondRequestTimesOutEarlierThanFirstRequest) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop sms_loop1, sms_loop2;

  auto* dialog1 = new NiceMock<MockSmsDialog>();
  auto* dialog2 = new NiceMock<MockSmsDialog>();

  SmsDialog::EventHandler hdl;

  EXPECT_CALL(*service.delegate(), CreateSmsDialog(_))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog1))))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog2))));

  EXPECT_CALL(*dialog1, Open(main_rfh(), _))
      .WillOnce(
          Invoke([&hdl](RenderFrameHost*, SmsDialog::EventHandler handler) {
            hdl = std::move(handler);
          }));

  EXPECT_CALL(*dialog2, Open(main_rfh(), _))
      .WillOnce(Invoke([](content::RenderFrameHost*,
                          content::SmsDialog::EventHandler event_handler) {
        // Simulates the user pressing "Try again".
        std::move(event_handler).Run(SmsDialog::Event::kTimeout);
      }));

  EXPECT_CALL(*dialog1, SmsReceived()).WillOnce(Invoke([&hdl]() {
    std::move(hdl).Run(SmsDialog::Event::kConfirm);
  }));

  EXPECT_CALL(*service.provider(), Retrieve())
      .WillOnce(Invoke([&service]() {
        // Delivers the first SMS.
        service.NotifyReceive(GURL(kTestUrl), "first");
      }))
      .WillOnce(Return());

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting(
          [&sms_loop1](SmsStatus status, const Optional<string>& sms) {
            EXPECT_EQ(SmsStatus::kSuccess, status);
            EXPECT_EQ("first", sms.value());
            sms_loop1.Quit();
          }));

  service.MakeRequest(
      TimeDelta::FromSeconds(0),
      BindLambdaForTesting(
          [&sms_loop2](SmsStatus status, const Optional<string>& sms) {
            EXPECT_EQ(SmsStatus::kTimeout, status);
            EXPECT_EQ(base::nullopt, sms);
            sms_loop2.Quit();
          }));

  // The second request immediately times out because it uses TimeDelta of 0
  // seconds.
  sms_loop2.Run();

  sms_loop1.Run();
}

TEST_F(SmsServiceTest, RecordTimeMetricsForContinueOnSuccess) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.SetupSmsDialog(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
  }));

  service.MakeRequest(
      TimeDelta::FromSeconds(10),
      BindLambdaForTesting(
          [&loop](SmsStatus status, const Optional<string>& sms) {
            loop.Quit();
          }));

  loop.Run();

  std::unique_ptr<base::HistogramSamples> continue_samples(
      GetHistogramSamplesSinceTestStart(
          "Blink.Sms.Receive.TimeContinueOnSuccess"));
  EXPECT_EQ(1, continue_samples->TotalCount());

  std::unique_ptr<base::HistogramSamples> receive_samples(
      GetHistogramSamplesSinceTestStart("Blink.Sms.Receive.TimeSmsReceive"));
  EXPECT_EQ(1, receive_samples->TotalCount());
}

TEST_F(SmsServiceTest, RecordMetricsForCancelOnSuccess) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  {
    // Histogram will not be recorded if the user cancels the operation before
    // the SMS arrives.
    base::RunLoop loop;

    service.MakeRequest(
        TimeDelta::FromSeconds(10),
        BindLambdaForTesting(
            [&loop](SmsStatus status, const Optional<string>& sms) {
              loop.Quit();
            }));

    auto* dialog = new NiceMock<MockSmsDialog>();

    EXPECT_CALL(*service.delegate(), CreateSmsDialog(_))
        .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

    EXPECT_CALL(*dialog, Open(main_rfh(), _))
        .WillOnce(Invoke([](RenderFrameHost*, SmsDialog::EventHandler handler) {
          // Simulates the user pressing "Cancel".
          std::move(handler).Run(SmsDialog::Event::kCancel);
        }));

    loop.Run();

    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            "Blink.Sms.Receive.TimeCancelOnSuccess"));
    EXPECT_EQ(0, samples->TotalCount());

    std::unique_ptr<base::HistogramSamples> receive_samples(
        GetHistogramSamplesSinceTestStart("Blink.Sms.Receive.TimeSmsReceive"));
    EXPECT_EQ(0, receive_samples->TotalCount());
  }

  {
    // Histogram will be recorded if the SMS has already arrived.
    base::RunLoop loop;

    EXPECT_CALL(*service.provider(), Retrieve()).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "hi");
    }));

    service.MakeRequest(
        TimeDelta::FromSeconds(10),
        BindLambdaForTesting(
            [&loop](SmsStatus status, const Optional<string>& sms) {
              loop.Quit();
            }));

    auto* dialog = new NiceMock<MockSmsDialog>();
    SmsDialog::EventHandler hdl;

    EXPECT_CALL(*service.delegate(), CreateSmsDialog(_))
        .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

    EXPECT_CALL(*dialog, Open(main_rfh(), _))
        .WillOnce(
            Invoke([&hdl](RenderFrameHost*, SmsDialog::EventHandler handler) {
              hdl = std::move(handler);
            }));

    EXPECT_CALL(*dialog, SmsReceived()).WillOnce(Invoke([&]() {
      // Simulates user clicking the "Cancel" once the SMS has been received.
      std::move(hdl).Run(SmsDialog::Event::kCancel);
    }));

    loop.Run();

    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            "Blink.Sms.Receive.TimeCancelOnSuccess"));
    EXPECT_EQ(1, samples->TotalCount());

    std::unique_ptr<base::HistogramSamples> receive_samples(
        GetHistogramSamplesSinceTestStart("Blink.Sms.Receive.TimeSmsReceive"));
    EXPECT_EQ(1, receive_samples->TotalCount());
  }
}

}  // namespace content
