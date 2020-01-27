// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class Profile;

// Trigger identifiers currently used; duplicates not allowed.
extern const char kHatsSurveyTriggerSatisfaction[];
extern const char kHatsSurveyTriggerSettings[];

// This class provides the client side logic for determining if a
// survey should be shown for any trigger based on input from a finch
// configuration. It is created on a per profile basis.
class HatsService : public KeyedService {
 public:
  struct SurveyConfig {
    SurveyConfig(const double probability, const std::string en_site_id)
        : probability_(probability), en_site_id_(en_site_id) {}

    SurveyConfig() = default;

    // Probability [0,1] of how likely a chosen user will see the survey.
    double probability_;

    // Site ID for the survey.
    std::string en_site_id_;
  };

  struct SurveyMetadata {
    SurveyMetadata();
    ~SurveyMetadata();

    base::Optional<int> last_major_version;
    base::Optional<base::Time> last_survey_started_time;
  };

  ~HatsService() override;

  explicit HatsService(Profile* profile);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Launch the survey with identifier |trigger| if appropriate.
  void LaunchSurvey(const std::string& trigger);

  void SetSurveyMetadataForTesting(const SurveyMetadata& metadata);

 private:
  // This returns true is the survey trigger specified should be shown.
  bool ShouldShowSurvey(const std::string& trigger) const;

  // Profile associated with this service.
  Profile* const profile_;

  base::flat_map<std::string, SurveyConfig> survey_configs_by_triggers_;

  DISALLOW_COPY_AND_ASSIGN(HatsService);
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
