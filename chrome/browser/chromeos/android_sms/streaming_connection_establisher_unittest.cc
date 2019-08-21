// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/streaming_connection_establisher.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace chromeos {

namespace android_sms {

class StreamingConnectionEstablisherTest : public testing::Test {
 protected:
  StreamingConnectionEstablisherTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~StreamingConnectionEstablisherTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
  DISALLOW_COPY_AND_ASSIGN(StreamingConnectionEstablisherTest);
};

TEST_F(StreamingConnectionEstablisherTest, EstablishConnection) {
  base::HistogramTester histogram_tester;
  content::FakeServiceWorkerContext fake_service_worker_context;
  base::SimpleTestClock test_clock;
  StreamingConnectionEstablisher connection_establisher(&test_clock);
  auto& message_dispatch_calls =
      fake_service_worker_context
          .start_service_worker_and_dispatch_long_running_message_calls();

  // Verify that long running message dispatch is called.
  connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, message_dispatch_calls.size());
  EXPECT_EQ(GetAndroidMessagesURL(), std::get<GURL>(message_dispatch_calls[0]));
  base::string16 message_string;
  blink::DecodeStringMessage(
      std::get<blink::TransferableMessage>(message_dispatch_calls[0])
          .owned_encoded_message,
      &message_string);
  EXPECT_EQ(
      base::UTF8ToUTF16(StreamingConnectionEstablisher::kStartStreamingMessage),
      message_string);

  // Verify that message is not dispatched again if previous result callback has
  // not returned yet.
  connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, message_dispatch_calls.size());

  // Return the result callback and verify that the metrics are recorded
  // properly.
  base::TimeDelta clock_advance = base::TimeDelta::FromMilliseconds(10);
  test_clock.Advance(clock_advance);
  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[0]))
      .Run(true);
  histogram_tester.ExpectUniqueSample("AndroidSms.ServiceWorkerLifetime",
                                      clock_advance.InMilliseconds(), 1);
  histogram_tester.ExpectBucketCount(
      "AndroidSms.ServiceWorkerMessageDispatchStatus", true, 1);

  // Verify that message is dispatched again if previous result callback already
  // returns.
  connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, message_dispatch_calls.size());

  std::move(std::get<content::ServiceWorkerContext::ResultCallback>(
                message_dispatch_calls[1]))
      .Run(true);
  connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kResumeExistingConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, message_dispatch_calls.size());
  EXPECT_EQ(GetAndroidMessagesURL(), std::get<GURL>(message_dispatch_calls[2]));
  base::string16 resume_message_string;
  blink::DecodeStringMessage(
      std::get<blink::TransferableMessage>(message_dispatch_calls[2])
          .owned_encoded_message,
      &resume_message_string);
  EXPECT_EQ(base::UTF8ToUTF16(
                StreamingConnectionEstablisher::kResumeStreamingMessage),
            resume_message_string);
}

TEST_F(StreamingConnectionEstablisherTest, TearDownConnection) {
  content::FakeServiceWorkerContext fake_service_worker_context;
  base::SimpleTestClock test_clock;
  StreamingConnectionEstablisher connection_establisher(&test_clock);

  // Verify that tear down stops all service worker.
  connection_establisher.TearDownConnection(GetAndroidMessagesURL(),
                                            &fake_service_worker_context);
  const auto& stop_calls =
      fake_service_worker_context.stop_all_service_workers_for_origin_calls();
  ASSERT_EQ(1u, stop_calls.size());
  EXPECT_EQ(GetAndroidMessagesURL().GetOrigin(), stop_calls[0]);

  // Verify that subsequent message message dispatch calls succeed.
  auto& message_dispatch_calls =
      fake_service_worker_context
          .start_service_worker_and_dispatch_long_running_message_calls();

  connection_establisher.EstablishConnection(
      GetAndroidMessagesURL(),
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      &fake_service_worker_context);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, message_dispatch_calls.size());
}

}  // namespace android_sms

}  // namespace chromeos
