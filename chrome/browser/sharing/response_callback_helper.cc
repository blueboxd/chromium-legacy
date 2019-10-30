// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/response_callback_helper.h"

#include "base/callback.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"

ResponseCallbackHelper::ResponseCallbackHelper() = default;
ResponseCallbackHelper::~ResponseCallbackHelper() = default;

void ResponseCallbackHelper::RegisterCallback(const std::string& message_guid,
                                              ResponseCallback callback) {
  DCHECK(!callbacks_.count(message_guid));
  callbacks_.emplace(message_guid, std::move(callback));
}

void ResponseCallbackHelper::RunCallback(
    const std::string& message_guid,
    chrome_browser_sharing::MessageType message_type,
    SharingSendMessageResult result,
    std::unique_ptr<SharingResponseMessage> response) {
  auto iter = callbacks_.find(message_guid);
  if (iter == callbacks_.end())
    return;

  LogSendSharingMessageResult(message_type, result);
  ResponseCallback callback = std::move(iter->second);
  callbacks_.erase(iter);
  std::move(callback).Run(result, std::move(response));
}

void ResponseCallbackHelper::OnFCMMessageSent(
    base::TimeTicks start_time,
    const std::string& message_guid,
    chrome_browser_sharing::MessageType message_type,
    SharingSendMessageResult result,
    base::Optional<std::string> fcm_message_id) {
  if (result != SharingSendMessageResult::kSuccessful) {
    RunCallback(message_guid, message_type, result, /*response=*/nullptr);
    return;
  }

  message_sent_time_.emplace(message_guid, start_time);
  message_guids_.emplace(*fcm_message_id, message_guid);
}

void ResponseCallbackHelper::OnFCMAckReceived(
    chrome_browser_sharing::MessageType message_type,
    const std::string& fcm_message_id,
    std::unique_ptr<SharingResponseMessage> response) {
  auto iter = message_guids_.find(fcm_message_id);
  if (iter == message_guids_.end())
    return;

  std::string message_guid = std::move(iter->second);
  message_guids_.erase(iter);

  auto times_iter = message_sent_time_.find(message_guid);
  if (times_iter != message_sent_time_.end()) {
    LogSharingMessageAckTime(message_type,
                             base::TimeTicks::Now() - times_iter->second);
    message_sent_time_.erase(times_iter);
  }

  RunCallback(message_guid, message_type, SharingSendMessageResult::kSuccessful,
              std::move(response));
}
