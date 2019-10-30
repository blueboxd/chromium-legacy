// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/response_callback_helper.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ResponseCallbackHelperTest, RunCallbackForUnregisteredGUID) {
  ResponseCallbackHelper callback_helper;
  base::HistogramTester histogram_tester;
  const std::string message_guid1 = base::GenerateGUID();
  const std::string message_guid2 = base::GenerateGUID();

  int times_called = 0;
  auto callback = base::BindLambdaForTesting(
      [&](SharingSendMessageResult result,
          std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
        times_called++;
      });

  callback_helper.RegisterCallback(message_guid1, std::move(callback));

  callback_helper.RunCallback(message_guid2,
                              chrome_browser_sharing::CLICK_TO_CALL_MESSAGE,
                              SharingSendMessageResult::kSuccessful,
                              /*response=*/nullptr);
  ASSERT_EQ(0, times_called);
  histogram_tester.ExpectTotalCount("Sharing.SendMessageResult", 0);

  callback_helper.RunCallback(message_guid1,
                              chrome_browser_sharing::CLICK_TO_CALL_MESSAGE,
                              SharingSendMessageResult::kSuccessful,
                              /*response=*/nullptr);
  EXPECT_EQ(1, times_called);
  histogram_tester.ExpectTotalCount("Sharing.SendMessageResult", 1);
}

TEST(ResponseCallbackHelperTest, RegisterAndRunMultipleCallbacks) {
  ResponseCallbackHelper callback_helper;
  base::HistogramTester histogram_tester;
  const std::string message_guid1 = base::GenerateGUID();
  const std::string message_guid2 = base::GenerateGUID();

  int times_called = 0;
  auto callback = base::BindLambdaForTesting(
      [&](SharingSendMessageResult result,
          std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
        times_called++;
      });

  callback_helper.RegisterCallback(message_guid1, std::move(callback));
  callback_helper.RegisterCallback(message_guid2, base::DoNothing());

  callback_helper.RunCallback(message_guid2,
                              chrome_browser_sharing::CLICK_TO_CALL_MESSAGE,
                              SharingSendMessageResult::kSuccessful,
                              /*response=*/nullptr);
  ASSERT_EQ(0, times_called);
  histogram_tester.ExpectTotalCount("Sharing.SendMessageResult", 1);

  callback_helper.RunCallback(message_guid1,
                              chrome_browser_sharing::CLICK_TO_CALL_MESSAGE,
                              SharingSendMessageResult::kSuccessful,
                              /*response=*/nullptr);
  EXPECT_EQ(1, times_called);
  histogram_tester.ExpectTotalCount("Sharing.SendMessageResult", 2);
}

TEST(ResponseCallbackHelperTest, SimulateSuccessfulMessageSent) {
  ResponseCallbackHelper callback_helper;
  base::HistogramTester histogram_tester;
  const std::string message_guid = "message_guid";
  const std::string fcm_message_id = "fcm_message_id";
  const chrome_browser_sharing::MessageType message_type =
      chrome_browser_sharing::CLICK_TO_CALL_MESSAGE;

  base::MockCallback<ResponseCallbackHelper::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kSuccessful),
                  testing::Eq(nullptr)));

  callback_helper.RegisterCallback(message_guid, mock_callback.Get());
  callback_helper.OnFCMMessageSent(
      base::TimeTicks::Now(), message_guid, message_type,
      SharingSendMessageResult::kSuccessful, fcm_message_id);

  callback_helper.OnFCMAckReceived(message_type, fcm_message_id, nullptr);

  const std::string ack_time_histogram_name =
      base::StrCat({"Sharing.MessageAckTime.",
                    chrome_browser_sharing::MessageType_Name(message_type)});

  histogram_tester.ExpectTotalCount("Sharing.SendMessageResult", 1);
  histogram_tester.ExpectTotalCount(ack_time_histogram_name, 1);
}
