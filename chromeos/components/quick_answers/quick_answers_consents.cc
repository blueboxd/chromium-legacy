// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_consents.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace quick_answers {

QuickAnswersConsent::QuickAnswersConsent(PrefService* prefs) : prefs_(prefs) {}

QuickAnswersConsent::~QuickAnswersConsent() = default;

void QuickAnswersConsent::StartConsent() {
  // Increments impression count.
  IncrementPrefCounter(prefs::kQuickAnswersConsentImpressionCount, 1);

  // Logs consent impression with how many times the user has seen the consent.
  RecordConsentImpression(GetImpressionCount());

  start_time_ = base::TimeTicks::Now();
}

void QuickAnswersConsent::DismissConsent() {
  RecordImpressionDuration();
  // Logs consent dismissed with impression count and impression duration.
  RecordConsentInteraction(ConsentInteractionType::kDismiss,
                           GetImpressionCount(), GetImpressionDuration());
}

void QuickAnswersConsent::AcceptConsent(ConsentInteractionType interaction) {
  RecordImpressionDuration();
  // Logs consent accepted with impression count and impression duration.
  RecordConsentInteraction(interaction, GetImpressionCount(),
                           GetImpressionDuration());

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
  return GetImpressionCount() + 1 > kConsentImpressionCap;
}

bool QuickAnswersConsent::HasReachedDurationCap() const {
  int duration_secs =
      prefs_->GetInteger(prefs::kQuickAnswersConsentImpressionDuration);
  return duration_secs >= kConsentDurationCap;
}

void QuickAnswersConsent::IncrementPrefCounter(const std::string& path,
                                               int count) {
  prefs_->SetInteger(path, prefs_->GetInteger(path) + count);
}

void QuickAnswersConsent::RecordImpressionDuration() {
  // Records duration in pref.
  IncrementPrefCounter(prefs::kQuickAnswersConsentImpressionDuration,
                       GetImpressionDuration().InSeconds());
}

int QuickAnswersConsent::GetImpressionCount() const {
  return prefs_->GetInteger(prefs::kQuickAnswersConsentImpressionCount);
}

base::TimeDelta QuickAnswersConsent::GetImpressionDuration() const {
  DCHECK(!start_time_.is_null());
  return base::TimeTicks::Now() - start_time_;
}

}  // namespace quick_answers
}  // namespace chromeos
