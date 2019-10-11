// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/signin_browsertest.h"

#include "chrome/browser/signin/scoped_account_consistency.h"
#include "components/unified_consent/scoped_unified_consent.h"

SigninBrowserTest::SigninBrowserTest() {
  EnableUnity();
}

SigninBrowserTest::~SigninBrowserTest() {}

void SigninBrowserTest::EnableDice() {
  scoped_account_consistency_ = std::make_unique<ScopedAccountConsistency>(
      signin::AccountConsistencyMethod::kDice);
}

void SigninBrowserTest::EnableUnity() {
  EnableDice();
  scoped_unified_consent_ =
      std::make_unique<unified_consent::ScopedUnifiedConsent>(
          unified_consent::UnifiedConsentFeatureState::kEnabled);
}
