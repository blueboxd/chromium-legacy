// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_H_

#include "base/feature_list.h"
#include "extensions/buildflags/buildflags.h"

namespace supervised_users {

extern const base::Feature kSupervisedUserIframeFilter;

extern const base::Feature kSupervisedUserInitiatedExtensionInstall;

#if BUILDFLAG(ENABLE_EXTENSIONS)
// For the COVID-19 crisis. Temporarily enable supervised users to install
// extensions from a policy allowlist without parental approval.
// TODO(crbug/1063104): Remove after the parent permission dialog launches.
extern const base::Feature kSupervisedUserAllowlistExtensionInstall;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace supervised_users

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_H_
