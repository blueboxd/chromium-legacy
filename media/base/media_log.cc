// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_log.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace media {

// A count of all MediaLogs created in the current process. Used to generate
// unique IDs.
static base::AtomicSequenceNumber g_media_log_count;

std::string MediaLog::MediaLogLevelToString(MediaLogLevel level) {
  switch (level) {
    case MEDIALOG_ERROR:
      return "error";
    case MEDIALOG_WARNING:
      return "warning";
    case MEDIALOG_INFO:
      return "info";
    case MEDIALOG_DEBUG:
      return "debug";
  }
  NOTREACHED();
  return NULL;
}

MediaLogRecord::Type MediaLog::MediaLogLevelToEventType(MediaLogLevel level) {
  switch (level) {
    case MEDIALOG_ERROR:
      return MediaLogRecord::MEDIA_ERROR_LOG_ENTRY;
    case MEDIALOG_WARNING:
      return MediaLogRecord::MEDIA_WARNING_LOG_ENTRY;
    case MEDIALOG_INFO:
      return MediaLogRecord::MEDIA_INFO_LOG_ENTRY;
    case MEDIALOG_DEBUG:
      return MediaLogRecord::MEDIA_DEBUG_LOG_ENTRY;
  }
  NOTREACHED();
  return MediaLogRecord::MEDIA_ERROR_LOG_ENTRY;
}

std::string MediaLog::EventTypeToString(MediaLogRecord::Type type) {
  switch (type) {
    case MediaLogRecord::WEBMEDIAPLAYER_CREATED:
      return "WEBMEDIAPLAYER_CREATED";
    case MediaLogRecord::WEBMEDIAPLAYER_DESTROYED:
      return "WEBMEDIAPLAYER_DESTROYED";
    case MediaLogRecord::LOAD:
      return "LOAD";
    case MediaLogRecord::SEEK:
      return "SEEK";
    case MediaLogRecord::PLAY:
      return "PLAY";
    case MediaLogRecord::PAUSE:
      return "PAUSE";
    case MediaLogRecord::PIPELINE_STATE_CHANGED:
      return "PIPELINE_STATE_CHANGED";
    case MediaLogRecord::PIPELINE_ERROR:
      return "PIPELINE_ERROR";
    case MediaLogRecord::VIDEO_SIZE_SET:
      return "VIDEO_SIZE_SET";
    case MediaLogRecord::DURATION_SET:
      return "DURATION_SET";
    case MediaLogRecord::ENDED:
      return "ENDED";
    case MediaLogRecord::TEXT_ENDED:
      return "TEXT_ENDED";
    case MediaLogRecord::MEDIA_ERROR_LOG_ENTRY:
      return "MEDIA_ERROR_LOG_ENTRY";
    case MediaLogRecord::MEDIA_WARNING_LOG_ENTRY:
      return "MEDIA_WARNING_LOG_ENTRY";
    case MediaLogRecord::MEDIA_INFO_LOG_ENTRY:
      return "MEDIA_INFO_LOG_ENTRY";
    case MediaLogRecord::MEDIA_DEBUG_LOG_ENTRY:
      return "MEDIA_DEBUG_LOG_ENTRY";
    case MediaLogRecord::PROPERTY_CHANGE:
      return "PROPERTY_CHANGE";
    case MediaLogRecord::BUFFERING_STATE_CHANGE:
      return "BUFFERING_STATE_CHANGE";
    case MediaLogRecord::SUSPENDED:
      return "SUSPENDED";
  }
  NOTREACHED();
  return NULL;
}

std::string MediaLog::MediaEventToLogString(const MediaLogRecord& event) {
  // Special case for PIPELINE_ERROR, since that's by far the most useful
  // event for figuring out media pipeline failures, and just reporting
  // pipeline status as numeric code is not very helpful/user-friendly.
  int error_code = 0;
  if (event.type == MediaLogRecord::PIPELINE_ERROR &&
      event.params.GetInteger("pipeline_error", &error_code)) {
    PipelineStatus status = static_cast<PipelineStatus>(error_code);
    return EventTypeToString(event.type) + " " + PipelineStatusToString(status);
  }

  std::string params_json;
  base::JSONWriter::Write(event.params, &params_json);
  return EventTypeToString(event.type) + " " + params_json;
}

std::string MediaLog::MediaEventToMessageString(const MediaLogRecord& event) {
  switch (event.type) {
    case MediaLogRecord::PIPELINE_ERROR: {
      int error_code = 0;
      event.params.GetInteger("pipeline_error", &error_code);
      DCHECK_NE(error_code, 0);
      return PipelineStatusToString(static_cast<PipelineStatus>(error_code));
    }
    case MediaLogRecord::MEDIA_ERROR_LOG_ENTRY: {
      std::string result = "";
      if (event.params.GetString(MediaLogLevelToString(MEDIALOG_ERROR),
                                 &result))
        base::ReplaceChars(result, "\n", " ", &result);
      return result;
    }
    default:
      NOTREACHED();
      return "";
  }
}

std::string MediaLog::BufferingStateToString(
    BufferingState state,
    BufferingStateChangeReason reason) {
  DCHECK(state == BUFFERING_HAVE_NOTHING || state == BUFFERING_HAVE_ENOUGH);
  DCHECK(reason == BUFFERING_CHANGE_REASON_UNKNOWN ||
         reason == DEMUXER_UNDERFLOW || reason == DECODER_UNDERFLOW ||
         reason == REMOTING_NETWORK_CONGESTION);

  std::string state_string = state == BUFFERING_HAVE_NOTHING
                                 ? "BUFFERING_HAVE_NOTHING"
                                 : "BUFFERING_HAVE_ENOUGH";

  std::vector<std::string> flag_strings;
  if (reason == DEMUXER_UNDERFLOW)
    state_string += " (DEMUXER_UNDERFLOW)";
  else if (reason == DECODER_UNDERFLOW)
    state_string += " (DECODER_UNDERFLOW)";
  else if (reason == REMOTING_NETWORK_CONGESTION)
    state_string += " (REMOTING_NETWORK_CONGESTION)";

  return state_string;
}

MediaLog::MediaLog() : MediaLog(new ParentLogRecord(this)) {}

MediaLog::MediaLog(scoped_refptr<ParentLogRecord> parent_log_record)
    : parent_log_record_(std::move(parent_log_record)),
      id_(g_media_log_count.GetNext()) {}

MediaLog::~MediaLog() {
  // If we are the underlying log, then somebody should have called
  // InvalidateLog before now.  Otherwise, there could be concurrent calls into
  // this after we're destroyed.  Note that calling it here isn't really much
  // better, since there could be concurrent calls into the now destroyed
  // derived class.
  //
  // However, we can't DCHECK on it, since lots of folks create a base Medialog
  // implementation temporarily.  So, the best we can do is invalidate the log.
  // We could get around this if we introduce a new NullMediaLog that handles
  // log invalidation, so we could dcheck here.  However, that seems like a lot
  // of boilerplate.
  if (parent_log_record_->media_log == this)
    InvalidateLog();
}

void MediaLog::OnWebMediaPlayerDestroyed() {
  AddLogRecord(CreateRecord(MediaLogRecord::WEBMEDIAPLAYER_DESTROYED));
  base::AutoLock auto_lock(parent_log_record_->lock);
  // Forward to the parent log's implementation.
  if (parent_log_record_->media_log)
    parent_log_record_->media_log->OnWebMediaPlayerDestroyedLocked();
}

void MediaLog::OnWebMediaPlayerDestroyedLocked() {}

void MediaLog::AddLogRecord(std::unique_ptr<MediaLogRecord> event) {
  base::AutoLock auto_lock(parent_log_record_->lock);
  // Forward to the parent log's implementation.
  if (parent_log_record_->media_log)
    parent_log_record_->media_log->AddLogRecordLocked(std::move(event));
}

void MediaLog::AddLogRecordLocked(std::unique_ptr<MediaLogRecord> event) {}

std::string MediaLog::GetErrorMessage() {
  base::AutoLock auto_lock(parent_log_record_->lock);
  // Forward to the parent log's implementation.
  if (parent_log_record_->media_log)
    return parent_log_record_->media_log->GetErrorMessageLocked();

  return "";
}

std::string MediaLog::GetErrorMessageLocked() {
  return "";
}

std::unique_ptr<MediaLogRecord> MediaLog::CreateCreatedEvent(
    const std::string& origin_url) {
  std::unique_ptr<MediaLogRecord> event(
      CreateRecord(MediaLogRecord::WEBMEDIAPLAYER_CREATED));
  event->params.SetString("origin_url", TruncateUrlString(origin_url));
  return event;
}

std::unique_ptr<MediaLogRecord> MediaLog::CreateRecord(
    MediaLogRecord::Type type) {
  std::unique_ptr<MediaLogRecord> event(new MediaLogRecord);
  event->id = id_;
  event->type = type;
  event->time = base::TimeTicks::Now();
  return event;
}

std::unique_ptr<MediaLogRecord> MediaLog::CreateBooleanEvent(
    MediaLogRecord::Type type,
    const std::string& property,
    bool value) {
  std::unique_ptr<MediaLogRecord> event(CreateRecord(type));
  event->params.SetBoolean(property, value);
  return event;
}

std::unique_ptr<MediaLogRecord> MediaLog::CreateStringEvent(
    MediaLogRecord::Type type,
    const std::string& property,
    const std::string& value) {
  std::unique_ptr<MediaLogRecord> event(CreateRecord(type));
  event->params.SetString(property, value);
  return event;
}

std::unique_ptr<MediaLogRecord> MediaLog::CreateTimeEvent(
    MediaLogRecord::Type type,
    const std::string& property,
    base::TimeDelta value) {
  return CreateTimeEvent(type, property, value.InSecondsF());
}

std::unique_ptr<MediaLogRecord> MediaLog::CreateTimeEvent(
    MediaLogRecord::Type type,
    const std::string& property,
    double value) {
  std::unique_ptr<MediaLogRecord> event(CreateRecord(type));
  if (std::isfinite(value))
    event->params.SetDouble(property, value);
  else
    event->params.SetString(property, "unknown");
  return event;
}

std::unique_ptr<MediaLogRecord> MediaLog::CreateLoadEvent(
    const std::string& url) {
  std::unique_ptr<MediaLogRecord> event(CreateRecord(MediaLogRecord::LOAD));
  event->params.SetString("url", TruncateUrlString(url));
  return event;
}

std::unique_ptr<MediaLogRecord> MediaLog::CreatePipelineStateChangedEvent(
    PipelineImpl::State state) {
  std::unique_ptr<MediaLogRecord> event(
      CreateRecord(MediaLogRecord::PIPELINE_STATE_CHANGED));
  event->params.SetString("pipeline_state",
                          PipelineImpl::GetStateString(state));
  return event;
}

std::unique_ptr<MediaLogRecord> MediaLog::CreatePipelineErrorEvent(
    PipelineStatus error) {
  std::unique_ptr<MediaLogRecord> event(
      CreateRecord(MediaLogRecord::PIPELINE_ERROR));
  event->params.SetInteger("pipeline_error", error);
  return event;
}

std::unique_ptr<MediaLogRecord> MediaLog::CreateVideoSizeSetEvent(
    size_t width,
    size_t height) {
  std::unique_ptr<MediaLogRecord> event(
      CreateRecord(MediaLogRecord::VIDEO_SIZE_SET));
  event->params.SetInteger("width", width);
  event->params.SetInteger("height", height);
  return event;
}

std::unique_ptr<MediaLogRecord> MediaLog::CreateBufferingStateChangedEvent(
    const std::string& property,
    BufferingState state,
    BufferingStateChangeReason reason) {
  return CreateStringEvent(MediaLogRecord::BUFFERING_STATE_CHANGE, property,
                           BufferingStateToString(state, reason));
}

void MediaLog::AddLogEvent(MediaLogLevel level, const std::string& message) {
  std::unique_ptr<MediaLogRecord> event(
      CreateRecord(MediaLogLevelToEventType(level)));
  event->params.SetString(MediaLogLevelToString(level), message);
  AddLogRecord(std::move(event));
}

std::unique_ptr<MediaLog> MediaLog::Clone() {
  // Protected ctor, so we can't use std::make_unique.
  return base::WrapUnique(new MediaLog(parent_log_record_));
}

// static
std::string MediaLog::TruncateUrlString(std::string log_string) {
  if (log_string.length() > kMaxUrlLength) {
    log_string.resize(kMaxUrlLength);

    // Room for the ellipsis.
    DCHECK_GE(kMaxUrlLength, std::size_t{3});
    log_string.replace(log_string.end() - 3, log_string.end(), "...");
  }

  return log_string;
}

MediaLog::ParentLogRecord::ParentLogRecord(MediaLog* log) : media_log(log) {}
MediaLog::ParentLogRecord::~ParentLogRecord() = default;

void MediaLog::InvalidateLog() {
  base::AutoLock auto_lock(parent_log_record_->lock);
  // It's almost certainly unintentional to invalidate a parent log.
  DCHECK(parent_log_record_->media_log == nullptr ||
         parent_log_record_->media_log == this);

  parent_log_record_->media_log = nullptr;
  // Keep |parent_log_record_| around, since the lock must keep working.
}

LogHelper::LogHelper(MediaLog::MediaLogLevel level, MediaLog* media_log)
    : level_(level), media_log_(media_log) {
  DCHECK(media_log_);
}

LogHelper::LogHelper(MediaLog::MediaLogLevel level,
                     const std::unique_ptr<MediaLog>& media_log)
    : LogHelper(level, media_log.get()) {}

LogHelper::~LogHelper() {
  media_log_->AddLogEvent(level_, stream_.str());
}

}  //namespace media
