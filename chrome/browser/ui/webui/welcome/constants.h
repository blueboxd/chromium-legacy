// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_CONSTANTS_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_CONSTANTS_H_

#include <string>
#include "base/metrics/field_trial_params.h"

namespace base {
struct Feature;
}  // namespace base

namespace nux {

extern const base::Feature kNuxOnboardingFeature;

extern const char kDefaultNewUserModules[];
extern const char kDefaultReturningUserModules[];

extern const base::FeatureParam<std::string> kNuxOnboardingNewUserModules;
extern const base::FeatureParam<std::string> kNuxOnboardingReturningUserModules;
extern const base::FeatureParam<bool> kNuxOnboardingShowGoogleApp;

}  // namespace nux

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_CONSTANTS_H_
