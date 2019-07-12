// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_SERVICE_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/accessibility/ax_mode.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Manages the feature that generates automatic image labels for accessibility.
// Tracks the per-profile preference and updates the accessibility mode of
// WebContents when it changes, provided image labeling is not disabled via
// command-line switch.
class AccessibilityLabelsService : public KeyedService {
 public:
  ~AccessibilityLabelsService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Off the record profiles will default to having the feature disabled.
  static void InitOffTheRecordPrefs(Profile* off_the_record_profile);

  void Init();

  ui::AXMode GetAXMode();

  void EnableLabelsServiceOnce();

 private:
  friend class AccessibilityLabelsServiceFactory;

  // Use |AccessibilityLabelsServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  explicit AccessibilityLabelsService(Profile* profile);

  void OnImageLabelsEnabledChanged();

  void UpdateAccessibilityLabelsHistograms();

  // Owns us via the KeyedService mechanism.
  Profile* profile_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<AccessibilityLabelsService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AccessibilityLabelsService);
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_SERVICE_H_
