// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_consents.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace quick_answers {

namespace {

constexpr int kImpressionCap = 3;
constexpr int kDurationCap = 8;

}  // namespace

QuickAnswersConsent::QuickAnswersConsent(PrefService* prefs) : prefs_(prefs) {}

QuickAnswersConsent::~QuickAnswersConsent() = default;

void QuickAnswersConsent::StartConsent() {
  // Increments impression count.
  IncrementPrefCounter(prefs::kQuickAnswersConsentImpressionCount, 1);
  start_time_ = base::TimeTicks::Now();
}

void QuickAnswersConsent::DismissConsent() {
  RecordImpressionDuration();
}

void QuickAnswersConsent::AcceptConsent() {
  RecordImpressionDuration();
  // Marks the consent as accepted.
  prefs_->SetBoolean(prefs::kQuickAnswersConsented, true);
}

bool QuickAnswersConsent::ShouldShowConsent() const {
  return !HasConsented() && !HasReachedImpressionCap() &&
         !HasReachedDurationCap();
}

bool QuickAnswersConsent::HasConsented() const {
  return prefs_->GetBoolean(prefs::kQuickAnswersConsented);
}

bool QuickAnswersConsent::HasReachedImpressionCap() const {
  int impression_count =
      prefs_->GetInteger(prefs::kQuickAnswersConsentImpressionCount);
  return impression_count + 1 > kImpressionCap;
}

bool QuickAnswersConsent::HasReachedDurationCap() const {
  int duration_secs =
      prefs_->GetInteger(prefs::kQuickAnswersConsentImpressionDuration);
  return duration_secs >= kDurationCap;
}

void QuickAnswersConsent::IncrementPrefCounter(const std::string& path,
                                               int count) {
  prefs_->SetInteger(path, prefs_->GetInteger(path) + count);
}

void QuickAnswersConsent::RecordImpressionDuration() {
  DCHECK(!start_time_.is_null());

  // Records duration in pref.
  base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
  IncrementPrefCounter(prefs::kQuickAnswersConsentImpressionDuration,
                       duration.InSeconds());
}

}  // namespace quick_answers
}  // namespace chromeos
