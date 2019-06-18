// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/proto_conversion.h"

#include <memory>
#include <utility>

#include "base/logging.h"

namespace notifications {

namespace {

// Helper method to convert base::TimeDelta to integer for serialization. Loses
// precision beyond miliseconds.
int64_t TimeDeltaToMilliseconds(const base::TimeDelta& delta) {
  return delta.InMilliseconds();
}

// Helper method to convert serialized time delta as integer to base::TimeDelta
// for deserialization. Loses precision beyond miliseconds.
base::TimeDelta MillisecondsToTimeDelta(int64_t serialized_delat_ms) {
  return base::TimeDelta::FromMilliseconds(serialized_delat_ms);
}

// Helper method to convert base::Time to integer for serialization. Loses
// precision beyond miliseconds.
int64_t TimeToMilliseconds(const base::Time& time) {
  return time.ToDeltaSinceWindowsEpoch().InMilliseconds();
}

// Helper method to convert serialized time as integer to base::Time for
// deserialization. Loses precision beyond miliseconds.
base::Time MillisecondsToTime(int64_t serialized_time_ms) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMilliseconds(serialized_time_ms));
}

// Converts SchedulerClientType to its associated enum in proto buffer.
proto::SchedulerClientType ToSchedulerClientType(SchedulerClientType type) {
  switch (type) {
    case SchedulerClientType::kTest1:
      return proto::SchedulerClientType::TEST_1;
    case SchedulerClientType::kTest2:
      return proto::SchedulerClientType::TEST_2;
    case SchedulerClientType::kTest3:
      return proto::SchedulerClientType::TEST_3;
    case SchedulerClientType::kUnknown:
      return proto::SchedulerClientType::UNKNOWN;
    case SchedulerClientType::kWebUI:
      return proto::SchedulerClientType::WEBUI;
  }
  NOTREACHED();
}

// Converts SchedulerClientType from its associated enum in proto buffer.
SchedulerClientType FromSchedulerClientType(
    proto::SchedulerClientType proto_type) {
  switch (proto_type) {
    case proto::SchedulerClientType::TEST_1:
      return SchedulerClientType::kTest1;
    case proto::SchedulerClientType::TEST_2:
      return SchedulerClientType::kTest2;
    case proto::SchedulerClientType::TEST_3:
      return SchedulerClientType::kTest3;
    case proto::SchedulerClientType::UNKNOWN:
      return SchedulerClientType::kUnknown;
    case proto::SchedulerClientType::WEBUI:
      return SchedulerClientType::kWebUI;
  }
  NOTREACHED();
}

// Converts UserFeedback to its associated enum in proto buffer.
proto::Impression_UserFeedback ToUserFeedback(UserFeedback feedback) {
  switch (feedback) {
    case UserFeedback::kNoFeedback:
      return proto::Impression_UserFeedback_NO_FEEDBACK;
    case UserFeedback::kHelpful:
      return proto::Impression_UserFeedback_HELPFUL;
    case UserFeedback::kNotHelpful:
      return proto::Impression_UserFeedback_NOT_HELPFUL;
    case UserFeedback::kClick:
      return proto::Impression_UserFeedback_CLICK;
    case UserFeedback::kDismiss:
      return proto::Impression_UserFeedback_DISMISS;
    case UserFeedback::kIgnore:
      return proto::Impression_UserFeedback_IGNORE;
  }
  NOTREACHED();
}

// Converts UserFeedback from its associated enum in proto buffer.
UserFeedback FromUserFeedback(proto::Impression_UserFeedback feedback) {
  switch (feedback) {
    case proto::Impression_UserFeedback_NO_FEEDBACK:
      return UserFeedback::kNoFeedback;
    case proto::Impression_UserFeedback_HELPFUL:
      return UserFeedback::kHelpful;
    case proto::Impression_UserFeedback_NOT_HELPFUL:
      return UserFeedback::kNotHelpful;
    case proto::Impression_UserFeedback_CLICK:
      return UserFeedback::kClick;
    case proto::Impression_UserFeedback_DISMISS:
      return UserFeedback::kDismiss;
    case proto::Impression_UserFeedback_IGNORE:
      return UserFeedback::kIgnore;
  }
  NOTREACHED();
}

// Converts ImpressionResult to its associated enum in proto buffer.
proto::Impression_ImpressionResult ToImpressionResult(ImpressionResult result) {
  switch (result) {
    case ImpressionResult::kInvalid:
      return proto::Impression_ImpressionResult_INVALID;
    case ImpressionResult::kPositive:
      return proto::Impression_ImpressionResult_POSITIVE;
    case ImpressionResult::kNegative:
      return proto::Impression_ImpressionResult_NEGATIVE;
    case ImpressionResult::kNeutral:
      return proto::Impression_ImpressionResult_NEUTRAL;
  }
  NOTREACHED();
}

// Converts ImpressionResult from its associated enum in proto buffer.
ImpressionResult FromImpressionResult(
    proto::Impression_ImpressionResult result) {
  switch (result) {
    case proto::Impression_ImpressionResult_INVALID:
      return ImpressionResult::kInvalid;
    case proto::Impression_ImpressionResult_POSITIVE:
      return ImpressionResult::kPositive;
    case proto::Impression_ImpressionResult_NEGATIVE:
      return ImpressionResult::kNegative;
    case proto::Impression_ImpressionResult_NEUTRAL:
      return ImpressionResult::kNeutral;
  }
  NOTREACHED();
}

// Converts ImpressionResult to its associated enum in proto buffer.
proto::Impression_SchedulerTaskTime ToSchedulerTaskTime(
    SchedulerTaskTime time) {
  switch (time) {
    case SchedulerTaskTime::kUnknown:
      return proto::Impression_SchedulerTaskTime_UNKNOWN_TASK_TIME;
    case SchedulerTaskTime::kMorning:
      return proto::Impression_SchedulerTaskTime_MORNING;
    case SchedulerTaskTime::kEvening:
      return proto::Impression_SchedulerTaskTime_EVENING;
  }
  NOTREACHED();
}

// Converts ImpressionResult from its associated enum in proto buffer.
SchedulerTaskTime FromSchedulerTaskTime(
    proto::Impression_SchedulerTaskTime time) {
  switch (time) {
    case proto::Impression_SchedulerTaskTime_UNKNOWN_TASK_TIME:
      return SchedulerTaskTime::kUnknown;
    case proto::Impression_SchedulerTaskTime_MORNING:
      return SchedulerTaskTime::kMorning;
    case proto::Impression_SchedulerTaskTime_EVENING:
      return SchedulerTaskTime::kEvening;
  }
  NOTREACHED();
}

// Converts NotificationData to proto buffer type.
void NotificationDataToProto(NotificationData* notification_data,
                             proto::NotificationData* proto) {
  proto->set_id(notification_data->id);
  proto->set_title(notification_data->title);
  proto->set_message(notification_data->message);
  proto->set_icon_uuid(notification_data->icon_uuid);
  proto->set_url(notification_data->url);
}

// Converts NotificationData from proto buffer type.
void NotificationDataFromProto(proto::NotificationData* proto,
                               NotificationData* notification_data) {
  notification_data->id = proto->id();
  notification_data->title = proto->title();
  notification_data->message = proto->message();
  notification_data->icon_uuid = proto->icon_uuid();
  notification_data->url = proto->url();
}

// Converts ScheduleParams::Priority to proto buffer type.
proto::ScheduleParams_Priority ScheduleParamsPriorityToProto(
    ScheduleParams::Priority priority) {
  using Priority = ScheduleParams::Priority;
  switch (priority) {
    case Priority::kLow:
      return proto::ScheduleParams_Priority_LOW;
    case Priority::kHigh:
      return proto::ScheduleParams_Priority_HIGH;
    case Priority::kNoThrottle:
      return proto::ScheduleParams_Priority_NO_THROTTLE;
  }
}

// Converts ScheduleParams::Priority from proto buffer type.
ScheduleParams::Priority ScheduleParamsPriorityFromProto(
    proto::ScheduleParams_Priority priority) {
  using Priority = ScheduleParams::Priority;
  switch (priority) {
    case proto::ScheduleParams_Priority_LOW:
      return Priority::kLow;
    case proto::ScheduleParams_Priority_HIGH:
      return Priority::kHigh;
    case proto::ScheduleParams_Priority_NO_THROTTLE:
      return Priority::kNoThrottle;
  }
}

// Converts ScheduleParams to proto buffer type.
void ScheduleParamsToProto(ScheduleParams* params,
                           proto::ScheduleParams* proto) {
  proto->set_priority(ScheduleParamsPriorityToProto(params->priority));
}

// Converts ScheduleParams from proto buffer type.
void ScheduleParamsFromProto(proto::ScheduleParams* proto,
                             ScheduleParams* params) {
  params->priority = ScheduleParamsPriorityFromProto(proto->priority());
}

}  // namespace

void IconEntryToProto(IconEntry* entry, notifications::proto::Icon* proto) {
  proto->mutable_uuid()->swap(entry->uuid);
  proto->mutable_icon()->swap(entry->data);
}

void IconEntryFromProto(proto::Icon* proto, notifications::IconEntry* entry) {
  DCHECK(proto->has_uuid());
  DCHECK(proto->has_icon());
  entry->data.swap(*proto->mutable_icon());
  entry->uuid.swap(*proto->mutable_uuid());
}

void ClientStateToProto(ClientState* client_state,
                        notifications::proto::ClientState* proto) {
  proto->set_type(ToSchedulerClientType(client_state->type));
  proto->set_current_max_daily_show(client_state->current_max_daily_show);

  for (const auto& impression : client_state->impressions) {
    auto* impression_ptr = proto->add_impressions();
    impression_ptr->set_create_time(TimeToMilliseconds(impression.create_time));
    impression_ptr->set_feedback(ToUserFeedback(impression.feedback));
    impression_ptr->set_impression(ToImpressionResult(impression.impression));
    impression_ptr->set_integrated(impression.integrated);
    impression_ptr->set_task_start_time(
        ToSchedulerTaskTime(impression.task_start_time));
    impression_ptr->set_guid(impression.guid);
  }

  if (client_state->suppression_info.has_value()) {
    const auto& suppression = *client_state->suppression_info;
    auto* suppression_proto = proto->mutable_suppression_info();
    suppression_proto->set_last_trigger_time(
        TimeToMilliseconds(suppression.last_trigger_time));
    suppression_proto->set_duration_ms(
        TimeDeltaToMilliseconds(suppression.duration));
    suppression_proto->set_recover_goal(suppression.recover_goal);
  }
}

void ClientStateFromProto(proto::ClientState* proto,
                          notifications::ClientState* client_state) {
  DCHECK(proto->has_type());
  DCHECK(proto->has_current_max_daily_show());
  client_state->type = FromSchedulerClientType(proto->type());
  client_state->current_max_daily_show = proto->current_max_daily_show();

  for (const auto& proto_impression : proto->impressions()) {
    Impression impression;
    DCHECK(proto_impression.has_create_time());
    impression.create_time = MillisecondsToTime(proto_impression.create_time());
    impression.feedback = FromUserFeedback(proto_impression.feedback());
    impression.impression = FromImpressionResult(proto_impression.impression());
    impression.integrated = proto_impression.integrated();
    impression.task_start_time =
        FromSchedulerTaskTime(proto_impression.task_start_time());
    impression.guid = proto_impression.guid();
    impression.type = client_state->type;
    client_state->impressions.emplace_back(std::move(impression));
  }

  if (proto->has_suppression_info()) {
    const auto& proto_suppression = proto->suppression_info();
    DCHECK(proto_suppression.has_last_trigger_time());
    DCHECK(proto_suppression.has_duration_ms());
    DCHECK(proto_suppression.has_recover_goal());

    SuppressionInfo suppression_info(
        MillisecondsToTime(proto_suppression.last_trigger_time()),
        MillisecondsToTimeDelta(proto_suppression.duration_ms()));
    suppression_info.recover_goal = proto_suppression.recover_goal();
    client_state->suppression_info = std::move(suppression_info);
  }
}

void NotificationEntryToProto(NotificationEntry* entry,
                              proto::NotificationEntry* proto) {
  proto->set_type(ToSchedulerClientType(entry->type));
  proto->set_guid(entry->guid);
  proto->set_create_time(TimeToMilliseconds(entry->create_time));
  auto* proto_notification_data = proto->mutable_notification_data();
  NotificationDataToProto(&entry->notification_data, proto_notification_data);
  auto* proto_schedule_params = proto->mutable_schedule_params();
  ScheduleParamsToProto(&entry->schedule_params, proto_schedule_params);
}

void NotificationEntryFromProto(proto::NotificationEntry* proto,
                                NotificationEntry* entry) {
  entry->type = FromSchedulerClientType(proto->type());
  entry->guid = proto->guid();
  entry->create_time = MillisecondsToTime(proto->create_time());
  NotificationDataFromProto(proto->mutable_notification_data(),
                            &entry->notification_data);
  ScheduleParamsFromProto(proto->mutable_schedule_params(),
                          &entry->schedule_params);
}

}  // namespace notifications
