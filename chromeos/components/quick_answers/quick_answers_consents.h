// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CONSENTS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CONSENTS_H_

#include <memory>

#include "base/timer/timer.h"

class PrefService;

namespace chromeos {
namespace quick_answers {

enum class ConsentInteractionType;

// The consent will appear up to a total of 3 times.
constexpr int kConsentImpressionCap = 3;
// The consent will appear until viewed for a cumulative 8 seconds.
constexpr int kConsentDurationCap = 8;

// Tracks whether quick answers consent should be shown and records impression
// count and duration when there is a interaction with the consent (shown,
// accepted and dismissed).
class QuickAnswersConsent {
 public:
  explicit QuickAnswersConsent(PrefService* prefs);

  QuickAnswersConsent(const QuickAnswersConsent&) = delete;
  QuickAnswersConsent& operator=(const QuickAnswersConsent&) = delete;

  virtual ~QuickAnswersConsent();

  // Starts showing consent. Virtual for testing.
  virtual void StartConsent();
  // Marks the consent as accepted and records the impression duration. Virtual
  // for testing.
  virtual void AcceptConsent(ConsentInteractionType interaction);
  // The consent is dismissed by users. Records the impression duration. Virtual
  // for testing.
  virtual void DismissConsent();
  // Whether the consent should be shown (based on consent state, impression
  // count and impression duration). Virtual for testing.
  virtual bool ShouldShowConsent() const;

 private:
  // Whether users have granted the consent.
  bool HasConsented() const;
  // Whether the consent has been seen by users for |kConsentImpressionCap|
  // times.
  bool HasReachedImpressionCap() const;
  // Whether the consent has been seen by users for |kConsentDurationCap|
  // seconds.
  bool HasReachedDurationCap() const;
  // Increments the perf counter by |count|.
  void IncrementPrefCounter(const std::string& path, int count);
  // Records how long the consent has been seen by the users.
  void RecordImpressionDuration();
  // Get how many times the consent has been seen by users.
  int GetImpressionCount() const;
  // Get how long the consent has been seen by users.
  base::TimeDelta GetImpressionDuration() const;

  PrefService* const prefs_;

  // Time when the consent is shown.
  base::TimeTicks start_time_;
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CONSENTS_H_
