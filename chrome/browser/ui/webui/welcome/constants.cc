// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/constants.h"

#include "base/feature_list.h"

namespace nux {

const base::Feature kNuxOnboardingFeature{"NuxOnboarding",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const char kDefaultNewUserModules[] =
    "nux-google-apps,nux-set-as-default,signin-view";
const char kDefaultReturningUserModules[] = "nux-set-as-default";

// The value of these FeatureParam values should be a comma-delimited list
// of element names whitelisted in the MODULES_WHITELIST list, defined in
// chrome/browser/resources/welcome/onboarding_welcome/welcome_app.js
const base::FeatureParam<std::string> kNuxOnboardingNewUserModules{
    &kNuxOnboardingFeature, "new-user-modules", kDefaultNewUserModules};
const base::FeatureParam<std::string> kNuxOnboardingReturningUserModules{
    &kNuxOnboardingFeature, "returning-user-modules",
    kDefaultReturningUserModules};

const base::FeatureParam<bool> kNuxOnboardingShowGoogleApp{
    &kNuxOnboardingFeature, "app-variation-enabled", false};

}  // namespace nux
