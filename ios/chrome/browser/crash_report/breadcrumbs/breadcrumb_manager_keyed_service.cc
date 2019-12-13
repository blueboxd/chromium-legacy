// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"

#include "base/strings/stringprintf.h"
#include "base/time/time_to_iso8601.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer.h"
#include "ios/web/public/browser_state.h"

namespace {

// The minimum number of event buckets to keep, even if they are expired.
const int kMinEventsBuckets = 2;

// Returns a Time used to bucket events for easier discarding of expired events.
base::Time EventBucket(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  exploded.millisecond = 0;
  exploded.second = 0;

  base::Time bucket_time;
  bool converted = base::Time::FromLocalExploded(exploded, &bucket_time);
  DCHECK(converted);
  return bucket_time;
}

}  // namespace

std::list<std::string> BreadcrumbManagerKeyedService::GetEvents(
    size_t event_count_limit) {
  DropOldEvents();

  std::list<std::string> events;
  for (auto it = event_buckets_.rbegin(); it != event_buckets_.rend(); ++it) {
    std::list<std::string> bucket_events = it->second;
    for (auto event_it = bucket_events.rbegin();
         event_it != bucket_events.rend(); ++event_it) {
      std::string event = *event_it;
      events.push_front(event);
      if (event_count_limit > 0 && events.size() >= event_count_limit) {
        return events;
      }
    }
  }
  return events;
}

void BreadcrumbManagerKeyedService::AddEvent(const std::string& event) {
  base::Time time = base::Time::Now();
  base::Time bucket_time = EventBucket(time);

  // If bucket exists, it will be at the end of the list.
  if (event_buckets_.empty() || event_buckets_.back().first != bucket_time) {
    std::pair<base::Time, std::list<std::string>> bucket(
        bucket_time, std::list<std::string>());
    event_buckets_.push_back(bucket);
  }

  std::string timestamp = base::TimeToISO8601(time);
  std::string event_log = base::StringPrintf(
      "%s %s %s", timestamp.c_str(), browsing_mode_.c_str(), event.c_str());
  event_buckets_.back().second.push_back(event_log);

  DropOldEvents();

  for (auto& observer : observers_) {
    observer.EventAdded(this, event_log);
  }
}

void BreadcrumbManagerKeyedService::DropOldEvents() {
  static const base::TimeDelta kMessageExpirationTime =
      base::TimeDelta::FromMinutes(20);

  base::Time now = base::Time::Now();
  while (event_buckets_.size() > kMinEventsBuckets) {
    base::Time oldest_bucket_time = event_buckets_.front().first;
    if (now - oldest_bucket_time < kMessageExpirationTime) {
      break;
    }
    event_buckets_.pop_front();
  }
}

BreadcrumbManagerKeyedService::BreadcrumbManagerKeyedService(
    web::BrowserState* browser_state)
    // Set "I" for Incognito (Chrome branded OffTheRecord implementation) and
    // "N" for Normal browsing mode.
    : browsing_mode_(browser_state->IsOffTheRecord() ? "I" : "N") {}

BreadcrumbManagerKeyedService::~BreadcrumbManagerKeyedService() = default;

void BreadcrumbManagerKeyedService::AddObserver(
    BreadcrumbManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BreadcrumbManagerKeyedService::RemoveObserver(
    BreadcrumbManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}
