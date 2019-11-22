// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/scoped_account_consistency.h"

#include <map>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "components/signin/public/base/signin_buildflags.h"

using signin::AccountConsistencyMethod;

ScopedAccountConsistencyMirror::ScopedAccountConsistencyMirror() {
#if BUILDFLAG(ENABLE_MIRROR)
  return;
#endif

  std::map<std::string, std::string> feature_params;
  feature_params[kAccountConsistencyFeatureMethodParameter] =
      kAccountConsistencyFeatureMethodMirror;

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kAccountConsistencyFeature, feature_params);
}

ScopedAccountConsistencyMirror::~ScopedAccountConsistencyMirror() {}
