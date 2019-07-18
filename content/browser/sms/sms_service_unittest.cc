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

using blink::mojom::SmsReceiverPtr;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace content {

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

  NiceMock<MockSmsProvider> provider;
  blink::mojom::SmsReceiverPtr service_ptr;
  auto service = std::make_unique<SmsService>(&provider, main_rfh(),
                                              mojo::MakeRequest(&service_ptr));

  base::RunLoop loop;

  EXPECT_CALL(provider, Retrieve()).WillOnce(Invoke([&provider]() {
    provider.NotifyReceive(url::Origin::Create(GURL(kTestUrl)), "hi");
  }));

  service_ptr->Receive(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
        EXPECT_EQ("hi", sms.value());
        EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, status);
        loop.Quit();
      }));

  loop.Run();

  ASSERT_FALSE(provider.HasObservers());
}

TEST_F(SmsServiceTest, ExpectTwoReceiveTwoSerially) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsProvider> provider;
  blink::mojom::SmsReceiverPtr service_ptr;
  auto service = std::make_unique<SmsService>(&provider, main_rfh(),
                                              mojo::MakeRequest(&service_ptr));

  EXPECT_CALL(provider, Retrieve())
      .WillOnce(Invoke([&provider]() {
        provider.NotifyReceive(url::Origin::Create(GURL(kTestUrl)), "first");
      }))
      .WillOnce(Invoke([&provider]() {
        provider.NotifyReceive(url::Origin::Create(GURL(kTestUrl)), "second");
      }));

  {
    base::RunLoop loop;

    service_ptr->Receive(
        base::TimeDelta::FromSeconds(10),
        base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                       const base::Optional<std::string>& sms) {
          EXPECT_EQ("first", sms.value());
          EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, status);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    service_ptr->Receive(
        base::TimeDelta::FromSeconds(10),
        base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                       const base::Optional<std::string>& sms) {
          EXPECT_EQ("second", sms.value());
          EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, status);
          loop.Quit();
        }));

    loop.Run();
  }
}

TEST_F(SmsServiceTest, IgnoreFromOtherOrigins) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsProvider> provider;
  blink::mojom::SmsReceiverPtr service_ptr;
  auto service = std::make_unique<SmsService>(&provider, main_rfh(),
                                              mojo::MakeRequest(&service_ptr));

  blink::mojom::SmsStatus sms_status;
  base::Optional<std::string> response;

  base::RunLoop listen_loop, sms_loop;

  EXPECT_CALL(provider, Retrieve()).WillOnce(Invoke([&listen_loop]() {
    listen_loop.Quit();
  }));

  service_ptr->Receive(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
        sms_status = status;
        response = sms;
        sms_loop.Quit();
      }));

  listen_loop.Run();

  // Delivers an SMS from an unrelated origin first and expect the receiver to
  // ignore it.
  GURL another_url("http://b.com");
  provider.NotifyReceive(url::Origin::Create(another_url), "wrong");

  provider.NotifyReceive(url::Origin::Create(GURL(kTestUrl)), "right");

  sms_loop.Run();

  EXPECT_EQ("right", response.value());
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, sms_status);
}

TEST_F(SmsServiceTest, ExpectOneReceiveTwo) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsProvider> provider;
  blink::mojom::SmsReceiverPtr service_ptr;
  auto service = std::make_unique<SmsService>(&provider, main_rfh(),
                                              mojo::MakeRequest(&service_ptr));

  blink::mojom::SmsStatus sms_status;
  base::Optional<std::string> response;

  base::RunLoop listen_loop, sms_loop;

  EXPECT_CALL(provider, Retrieve()).WillOnce(Invoke([&listen_loop]() {
    listen_loop.Quit();
  }));

  service_ptr->Receive(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
        sms_status = status;
        response = sms;
        sms_loop.Quit();
      }));

  listen_loop.Run();

  // Delivers two SMSes for the same origin, even if only one was being
  // expected.
  provider.NotifyReceive(url::Origin::Create(GURL(kTestUrl)), "first");
  provider.NotifyReceive(url::Origin::Create(GURL(kTestUrl)), "second");

  sms_loop.Run();

  EXPECT_EQ("first", response.value());
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, sms_status);
}

TEST_F(SmsServiceTest, ExpectTwoReceiveTwoConcurrently) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsProvider> provider;
  blink::mojom::SmsReceiverPtr service_ptr;
  auto service = std::make_unique<SmsService>(&provider, main_rfh(),
                                              mojo::MakeRequest(&service_ptr));

  blink::mojom::SmsStatus sms_status1;
  base::Optional<std::string> response1;
  blink::mojom::SmsStatus sms_status2;
  base::Optional<std::string> response2;

  base::RunLoop listen_loop, sms1_loop, sms2_loop;

  // Expects two Receive() calls to be made before any of them gets
  // an SMS to resolve them.
  EXPECT_CALL(provider, Retrieve())
      .WillOnce(testing::Return())
      .WillOnce(Invoke([&listen_loop]() { listen_loop.Quit(); }));

  service_ptr->Receive(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
        sms_status1 = status;
        response1 = sms;
        sms1_loop.Quit();
      }));

  service_ptr->Receive(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
        sms_status2 = status;
        response2 = sms;
        sms2_loop.Quit();
      }));

  listen_loop.Run();

  // Delivers the first SMS.

  provider.NotifyReceive(url::Origin::Create(GURL(kTestUrl)), "first");

  sms1_loop.Run();

  EXPECT_EQ("first", response1.value());
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, sms_status1);

  // Delivers the second SMS.

  provider.NotifyReceive(url::Origin::Create(GURL(kTestUrl)), "second");

  sms2_loop.Run();

  EXPECT_EQ("second", response2.value());
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, sms_status2);
}

TEST_F(SmsServiceTest, Timeout) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsProvider> provider;
  blink::mojom::SmsReceiverPtr service_ptr;
  auto service = std::make_unique<SmsService>(&provider, main_rfh(),
                                              mojo::MakeRequest(&service_ptr));

  base::RunLoop loop;

  service_ptr->Receive(
      base::TimeDelta::FromSeconds(0),
      base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
        EXPECT_EQ(blink::mojom::SmsStatus::kTimeout, status);
        loop.Quit();
      }));

  loop.Run();
}

TEST_F(SmsServiceTest, CleansUp) {
  NavigateAndCommit(GURL(kTestUrl));

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

TEST_F(SmsServiceTest, TimeoutTwoTabs) {
  NiceMock<MockSmsProvider> provider;

  blink::mojom::SmsReceiverPtr service_ptr1;
  GURL url1("http://a.com");
  auto tab1 = std::make_unique<SmsService>(&provider, url::Origin::Create(url1),
                                           main_rfh(),
                                           mojo::MakeRequest(&service_ptr1));

  blink::mojom::SmsReceiverPtr service_ptr2;
  GURL url2("http://b.com");
  auto tab2 = std::make_unique<SmsService>(&provider, url::Origin::Create(url2),
                                           main_rfh(),
                                           mojo::MakeRequest(&service_ptr2));

  blink::mojom::SmsStatus sms_status1;
  base::Optional<std::string> response1;
  blink::mojom::SmsStatus sms_status2;
  base::Optional<std::string> response2;

  base::RunLoop listen, sms_loop1, sms_loop2;

  EXPECT_CALL(provider, Retrieve())
      .WillOnce(testing::Return())
      .WillOnce(Invoke([&listen]() { listen.Quit(); }));

  service_ptr1->Receive(
      base::TimeDelta::FromSeconds(0),
      base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
        sms_status1 = status;
        response1 = sms;
        sms_loop1.Quit();
      }));

  service_ptr2->Receive(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
        sms_status2 = status;
        response2 = sms;
        sms_loop2.Quit();
      }));

  listen.Run();

  // The first request immediately times out because it uses TimeDelta of 0
  // seconds.

  sms_loop1.Run();

  EXPECT_EQ(blink::mojom::SmsStatus::kTimeout, sms_status1);

  // Delivers the second SMS.
  provider.NotifyReceive(url::Origin::Create(url2), "second");

  sms_loop2.Run();

  EXPECT_EQ("second", response2.value());
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, sms_status2);
}

TEST_F(SmsServiceTest, SecondRequestTimesOutEarlierThanFirstRequest) {
  NiceMock<MockSmsProvider> provider;

  GURL url1("http://a.com");
  blink::mojom::SmsReceiverPtr service_ptr1;
  std::unique_ptr<SmsService> service1 = std::make_unique<SmsService>(
      &provider, url::Origin::Create(url1), main_rfh(),
      mojo::MakeRequest(&service_ptr1));

  GURL url2("http://b.com");
  blink::mojom::SmsReceiverPtr service_ptr2;
  std::unique_ptr<SmsService> service2 = std::make_unique<SmsService>(
      &provider, url::Origin::Create(url2), main_rfh(),
      mojo::MakeRequest(&service_ptr2));

  blink::mojom::SmsStatus sms_status1;
  base::Optional<std::string> response1;
  blink::mojom::SmsStatus sms_status2;
  base::Optional<std::string> response2;

  base::RunLoop listen, sms_loop1, sms_loop2;

  EXPECT_CALL(provider, Retrieve())
      .WillOnce(testing::Return())
      .WillOnce(Invoke([&listen]() { listen.Quit(); }));

  service_ptr1->Receive(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
        sms_status1 = status;
        response1 = sms;
        sms_loop1.Quit();
      }));

  service_ptr2->Receive(
      base::TimeDelta::FromSeconds(0),
      base::BindLambdaForTesting([&](blink::mojom::SmsStatus status,
                                     const base::Optional<std::string>& sms) {
        sms_status2 = status;
        response2 = sms;
        sms_loop2.Quit();
      }));

  listen.Run();

  // The second request immediately times out because it uses TimeDelta of 0
  // seconds.

  sms_loop2.Run();

  EXPECT_EQ(blink::mojom::SmsStatus::kTimeout, sms_status2);

  // Delivers the first SMS.

  provider.NotifyReceive(url::Origin::Create(url1), "first");

  sms_loop1.Run();

  EXPECT_EQ("first", response1.value());
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, sms_status1);
}

}  // namespace content
