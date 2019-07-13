// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/escape.h"
#include "net/log/net_log_values.h"

namespace net {

namespace {

// IEEE 64-bit doubles have a 52-bit mantissa, and can therefore represent
// 53-bits worth of precision (see also documentation for JavaScript's
// Number.MAX_SAFE_INTEGER for more discussion on this).
//
// If the number can be represented with an int or double use that. Otherwise
// fallback to encoding it as a string.
template <typename T>
base::Value NetLogNumberValueHelper(T num) {
  // Fits in a (32-bit) int: [-2^31, 2^31 - 1]
  if ((!std::is_signed<T>::value || (num >= static_cast<T>(-2147483648))) &&
      (num <= static_cast<T>(2147483647))) {
    return base::Value(static_cast<int>(num));
  }

  // Fits in a double: (-2^53, 2^53)
  if ((!std::is_signed<T>::value ||
       (num >= static_cast<T>(-9007199254740991))) &&
      (num <= static_cast<T>(9007199254740991))) {
    return base::Value(static_cast<double>(num));
  }

  // Otherwise format as a string.
  return base::Value(base::NumberToString(num));
}

}  // namespace

NetLog::ThreadSafeObserver::ThreadSafeObserver()
    : capture_mode_(NetLogCaptureMode::kDefault), net_log_(nullptr) {}

NetLog::ThreadSafeObserver::~ThreadSafeObserver() {
  // Make sure we aren't watching a NetLog on destruction.  Because the NetLog
  // may pass events to each observer on multiple threads, we cannot safely
  // stop watching a NetLog automatically from a parent class.
  DCHECK(!net_log_);
}

NetLogCaptureMode NetLog::ThreadSafeObserver::capture_mode() const {
  DCHECK(net_log_);
  return capture_mode_;
}

NetLog* NetLog::ThreadSafeObserver::net_log() const {
  return net_log_;
}

NetLog::NetLog() : last_id_(0), observer_capture_modes_(0) {}

NetLog::~NetLog() = default;

void NetLog::AddEntry(NetLogEventType type,
                      const NetLogSource& source,
                      NetLogEventPhase phase) {
  AddEntry(type, source, phase, [] { return base::Value(); });
}

void NetLog::AddGlobalEntry(NetLogEventType type) {
  AddEntry(type, NetLogSource(NetLogSourceType::NONE, NextID()),
           NetLogEventPhase::NONE);
}

void NetLog::AddGlobalEntryWithStringParams(NetLogEventType type,
                                            base::StringPiece name,
                                            base::StringPiece value) {
  AddGlobalEntry(type, [&] { return NetLogParamsWithString(name, value); });
}

uint32_t NetLog::NextID() {
  return base::subtle::NoBarrier_AtomicIncrement(&last_id_, 1);
}

bool NetLog::IsCapturing() const {
  return GetObserverCaptureModes() != 0;
}

void NetLog::AddObserver(NetLog::ThreadSafeObserver* observer,
                         NetLogCaptureMode capture_mode) {
  base::AutoLock lock(lock_);

  DCHECK(!observer->net_log_);
  DCHECK(!HasObserver(observer));
  DCHECK_LT(observers_.size(), 20u);  // Performance sanity check.

  observers_.push_back(observer);

  observer->net_log_ = this;
  observer->capture_mode_ = capture_mode;
  UpdateObserverCaptureModes();
}

void NetLog::SetObserverCaptureMode(NetLog::ThreadSafeObserver* observer,
                                    NetLogCaptureMode capture_mode) {
  base::AutoLock lock(lock_);

  DCHECK(HasObserver(observer));
  DCHECK_EQ(this, observer->net_log_);
  observer->capture_mode_ = capture_mode;
  UpdateObserverCaptureModes();
}

void NetLog::RemoveObserver(NetLog::ThreadSafeObserver* observer) {
  base::AutoLock lock(lock_);

  DCHECK_EQ(this, observer->net_log_);

  auto it = std::find(observers_.begin(), observers_.end(), observer);
  DCHECK(it != observers_.end());
  observers_.erase(it);

  observer->net_log_ = nullptr;
  observer->capture_mode_ = NetLogCaptureMode::kDefault;
  UpdateObserverCaptureModes();
}

void NetLog::UpdateObserverCaptureModes() {
  lock_.AssertAcquired();

  NetLogCaptureModeSet capture_mode_set = 0;
  for (const auto* observer : observers_)
    NetLogCaptureModeSetAdd(observer->capture_mode_, &capture_mode_set);

  base::subtle::NoBarrier_Store(&observer_capture_modes_, capture_mode_set);
}

bool NetLog::HasObserver(ThreadSafeObserver* observer) {
  lock_.AssertAcquired();
  return base::Contains(observers_, observer);
}

// static
std::string NetLog::TickCountToString(const base::TimeTicks& time) {
  int64_t delta_time = time.since_origin().InMilliseconds();
  // TODO(https://crbug.com/915391): Use NetLogNumberValue().
  return base::NumberToString(delta_time);
}

// static
std::string NetLog::TimeToString(const base::Time& time) {
  // Convert the base::Time to its (approximate) equivalent in base::TimeTicks.
  base::TimeTicks time_ticks =
      base::TimeTicks::UnixEpoch() + (time - base::Time::UnixEpoch());
  return TickCountToString(time_ticks);
}

// static
const char* NetLog::EventTypeToString(NetLogEventType event) {
  switch (event) {
#define EVENT_TYPE(label)      \
  case NetLogEventType::label: \
    return #label;
#include "net/log/net_log_event_type_list.h"
#undef EVENT_TYPE
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
base::Value NetLog::GetEventTypesAsValue() {
  base::DictionaryValue dict;
  for (int i = 0; i < static_cast<int>(NetLogEventType::COUNT); ++i) {
    dict.SetInteger(EventTypeToString(static_cast<NetLogEventType>(i)), i);
  }
  return std::move(dict);
}

// static
const char* NetLog::SourceTypeToString(NetLogSourceType source) {
  switch (source) {
#define SOURCE_TYPE(label)      \
  case NetLogSourceType::label: \
    return #label;
#include "net/log/net_log_source_type_list.h"
#undef SOURCE_TYPE
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
base::Value NetLog::GetSourceTypesAsValue() {
  base::DictionaryValue dict;
  for (int i = 0; i < static_cast<int>(NetLogSourceType::COUNT); ++i) {
    dict.SetInteger(SourceTypeToString(static_cast<NetLogSourceType>(i)), i);
  }
  return std::move(dict);
}

// static
const char* NetLog::EventPhaseToString(NetLogEventPhase phase) {
  switch (phase) {
    case NetLogEventPhase::BEGIN:
      return "PHASE_BEGIN";
    case NetLogEventPhase::END:
      return "PHASE_END";
    case NetLogEventPhase::NONE:
      return "PHASE_NONE";
  }
  NOTREACHED();
  return nullptr;
}

void NetLog::AddEntryInternal(NetLogEventType type,
                              const NetLogSource& source,
                              NetLogEventPhase phase,
                              const GetParamsInterface* get_params) {
  NetLogCaptureModeSet observer_capture_modes = GetObserverCaptureModes();

  for (int i = 0; i <= static_cast<int>(NetLogCaptureMode::kLast); ++i) {
    NetLogCaptureMode capture_mode = static_cast<NetLogCaptureMode>(i);
    if (!NetLogCaptureModeSetContains(capture_mode, observer_capture_modes))
      continue;

    NetLogEntry entry(type, source, phase, base::TimeTicks::Now(),
                      get_params->GetParams(capture_mode));

    // Notify all of the log observers with |capture_mode|.
    base::AutoLock lock(lock_);
    for (auto* observer : observers_) {
      if (observer->capture_mode() == capture_mode)
        observer->OnAddEntry(entry);
    }
  }
}

NetLogCaptureModeSet NetLog::GetObserverCaptureModes() const {
  return base::subtle::NoBarrier_Load(&observer_capture_modes_);
}

void NetLog::AddEntryWithMaterializedParams(NetLogEventType type,
                                            const NetLogSource& source,
                                            NetLogEventPhase phase,
                                            base::Value&& params) {
  NetLogEntry entry(type, source, phase, base::TimeTicks::Now(),
                    std::move(params));

  // Notify all of the log observers with |capture_mode|.
  base::AutoLock lock(lock_);
  for (auto* observer : observers_) {
    observer->OnAddEntry(entry);
  }
}

base::Value NetLogStringValue(base::StringPiece raw) {
  // The common case is that |raw| is ASCII. Represent this directly.
  if (base::IsStringASCII(raw))
    return base::Value(raw);

  // For everything else (including valid UTF-8) percent-escape |raw|, and add a
  // prefix that "tags" the value as being a percent-escaped representation.
  //
  // Note that the sequence E2 80 8B is U+200B (zero-width space) in UTF-8. It
  // is added so the escaped string is not itself also ASCII (otherwise there
  // would be ambiguity for consumers as to when the value needs to be
  // unescaped).
  return base::Value("%ESCAPED:\xE2\x80\x8B " + EscapeNonASCIIAndPercent(raw));
}

base::Value NetLogBinaryValue(const void* bytes, size_t length) {
  std::string b64;
  Base64Encode(base::StringPiece(reinterpret_cast<const char*>(bytes), length),
               &b64);
  return base::Value(std::move(b64));
}

base::Value NetLogNumberValue(int64_t num) {
  return NetLogNumberValueHelper(num);
}

base::Value NetLogNumberValue(uint64_t num) {
  return NetLogNumberValueHelper(num);
}

}  // namespace net
