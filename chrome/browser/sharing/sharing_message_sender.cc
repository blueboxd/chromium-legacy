// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_message_sender.h"

#include "base/guid.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/sharing_utils.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "content/public/browser/browser_task_traits.h"

SharingMessageSender::SharingMessageSender(
    SharingSyncPreference* sync_prefs,
    syncer::LocalDeviceInfoProvider* local_device_info_provider)
    : sync_prefs_(sync_prefs),
      local_device_info_provider_(local_device_info_provider) {}

SharingMessageSender::~SharingMessageSender() = default;

void SharingMessageSender::SendMessageToDevice(
    const syncer::DeviceInfo& device,
    base::TimeDelta response_timeout,
    chrome_browser_sharing::SharingMessage message,
    DelegateType delegate_type,
    ResponseCallback callback) {
  DCHECK(message.payload_case() !=
         chrome_browser_sharing::SharingMessage::kAckMessage);

  int trace_id = GenerateSharingTraceId();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "sharing", "Sharing.SendMessage", TRACE_ID_LOCAL(trace_id),
      "message_type",
      SharingMessageTypeToString(
          SharingPayloadCaseToMessageType(message.payload_case())));

  std::string message_guid = base::GenerateGUID();
  chrome_browser_sharing::MessageType message_type =
      SharingPayloadCaseToMessageType(message.payload_case());
  SharingDevicePlatform receiver_device_platform =
      sync_prefs_->GetDevicePlatform(device.guid());
  base::TimeDelta last_updated_age =
      base::Time::Now() - device.last_updated_timestamp();

  auto inserted = base::InsertOrAssign(
      message_metadata_, message_guid,
      SentMessageMetadata(std::move(callback), base::TimeTicks::Now(),
                          message_type, receiver_device_platform,
                          last_updated_age, trace_id));
  DCHECK(inserted.second);

  auto delegate_iter = send_delegates_.find(delegate_type);
  if (delegate_iter == send_delegates_.end()) {
    InvokeSendMessageCallback(message_guid,
                              SharingSendMessageResult::kInternalError,
                              /*response=*/nullptr);
    return;
  }
  SendMessageDelegate* delegate = delegate_iter->second.get();
  DCHECK(delegate);

  // TODO(crbug/1015411): Here we assume the caller gets the |device| from
  // GetDeviceCandidates, so LocalDeviceInfoProvider is ready. It's better to
  // queue up the message and wait until LocalDeviceInfoProvider is ready.
  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  if (!local_device_info) {
    InvokeSendMessageCallback(message_guid,
                              SharingSendMessageResult::kInternalError,
                              /*response=*/nullptr);
    return;
  }

  base::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, content::BrowserThread::UI},
      base::BindOnce(&SharingMessageSender::InvokeSendMessageCallback,
                     weak_ptr_factory_.GetWeakPtr(), message_guid,
                     SharingSendMessageResult::kAckTimeout,
                     /*response=*/nullptr),
      response_timeout);

  LogSharingDeviceLastUpdatedAge(message_type, last_updated_age);
  LogSharingVersionComparison(message_type, device.chrome_version());

  message.set_sender_guid(local_device_info->guid());
  message.set_sender_device_name(
      send_tab_to_self::GetSharingDeviceNames(local_device_info).full_name);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("sharing", "Sharing.DoSendMessage",
                                    TRACE_ID_LOCAL(trace_id));
  delegate->DoSendMessageToDevice(
      device, response_timeout, std::move(message),
      base::BindOnce(&SharingMessageSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), message_guid));
}

void SharingMessageSender::OnMessageSent(
    const std::string& message_guid,
    SharingSendMessageResult result,
    base::Optional<std::string> message_id) {
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "sharing", "Sharing.DoSendMessage",
      TRACE_ID_LOCAL(message_metadata_.find(message_guid)->second.trace_id),
      "result", SharingSendMessageResultToString(result));
  if (result != SharingSendMessageResult::kSuccessful) {
    InvokeSendMessageCallback(message_guid, result,
                              /*response=*/nullptr);
    return;
  }

  // Got a new message id, store it for later.
  message_guids_.emplace(*message_id, message_guid);

  // Check if we got the ack while waiting for the FCM response.
  auto cached_iter = cached_ack_response_messages_.find(*message_id);
  if (cached_iter != cached_ack_response_messages_.end()) {
    OnAckReceived(*message_id, std::move(cached_iter->second));
    cached_ack_response_messages_.erase(cached_iter);
  }
}

void SharingMessageSender::OnAckReceived(
    const std::string& message_id,
    std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
  TRACE_EVENT0("sharing", "SharingMessageSender::OnAckReceived");
  auto guid_iter = message_guids_.find(message_id);
  if (guid_iter == message_guids_.end()) {
    // We don't have the guid yet, store the response until we receive it.
    cached_ack_response_messages_.emplace(message_id, std::move(response));
    return;
  }

  std::string message_guid = std::move(guid_iter->second);
  message_guids_.erase(guid_iter);

  auto metadata_iter = message_metadata_.find(message_guid);
  DCHECK(metadata_iter != message_metadata_.end());
  const SentMessageMetadata& metadata = metadata_iter->second;

  LogSharingMessageAckTime(metadata.type, metadata.receiver_device_platform,
                           base::TimeTicks::Now() - metadata.timestamp);

  InvokeSendMessageCallback(message_guid, SharingSendMessageResult::kSuccessful,
                            std::move(response));

  message_metadata_.erase(metadata_iter);
}

void SharingMessageSender::RegisterSendDelegate(
    DelegateType type,
    std::unique_ptr<SendMessageDelegate> delegate) {
  auto result = send_delegates_.emplace(type, std::move(delegate));
  DCHECK(result.second) << "Delegate type already registered";
}

void SharingMessageSender::InvokeSendMessageCallback(
    const std::string& message_guid,
    SharingSendMessageResult result,
    std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
  auto iter = message_metadata_.find(message_guid);
  if (iter == message_metadata_.end() || !iter->second.callback)
    return;

  SentMessageMetadata& metadata = iter->second;

  std::move(metadata.callback).Run(result, std::move(response));

  LogSendSharingMessageResult(metadata.type, metadata.receiver_device_platform,
                              result);
  LogSharingDeviceLastUpdatedAgeWithResult(result, metadata.last_updated_age);
  TRACE_EVENT_NESTABLE_ASYNC_END1("sharing", "SharingMessageSender.SendMessage",
                                  TRACE_ID_LOCAL(metadata.trace_id), "result",
                                  SharingSendMessageResultToString(result));
}

SharingMessageSender::SentMessageMetadata::SentMessageMetadata(
    ResponseCallback callback,
    base::TimeTicks timestamp,
    chrome_browser_sharing::MessageType type,
    SharingDevicePlatform receiver_device_platform,
    base::TimeDelta last_updated_age,
    int trace_id)
    : callback(std::move(callback)),
      timestamp(timestamp),
      type(type),
      receiver_device_platform(receiver_device_platform),
      last_updated_age(last_updated_age),
      trace_id(trace_id) {}

SharingMessageSender::SentMessageMetadata::SentMessageMetadata(
    SentMessageMetadata&& other) = default;

SharingMessageSender::SentMessageMetadata&
SharingMessageSender::SentMessageMetadata::operator=(
    SentMessageMetadata&& other) = default;

SharingMessageSender::SentMessageMetadata::~SentMessageMetadata() = default;
