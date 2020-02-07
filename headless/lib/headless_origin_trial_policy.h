// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_ORIGIN_TRIAL_POLICY_H_
#define HEADLESS_LIB_HEADLESS_ORIGIN_TRIAL_POLICY_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"

// This class implements an OriginTrialPolicy to allow origin trials to be
// enabled in headless mode. For more information on origin trials, see:
// 1)
// https://github.com/GoogleChrome/OriginTrials/blob/gh-pages/developer-guide.md
// 2)
// https://dev.chromium.org/blink/origin-trials/running-an-origin-trial#TOC-How-do-Origin-Trials-work-in-Chrome-
// This class is instantiated on the main/ui thread, but its methods can be
// accessed from any thread.
// TODO(crbug.com/1049317): Figure out how to share implementation with the
// other class ChromeOriginTrialPolicy.
class HeadlessOriginTrialPolicy : public blink::OriginTrialPolicy {
 public:
  HeadlessOriginTrialPolicy();
  ~HeadlessOriginTrialPolicy() override;

  // blink::OriginTrialPolicy interface
  bool IsOriginTrialsSupported() const override;
  std::vector<base::StringPiece> GetPublicKeys() const override;
  bool IsFeatureDisabled(base::StringPiece feature) const override;
  bool IsTokenDisabled(base::StringPiece token_signature) const override;
  bool IsOriginSecure(const GURL& url) const override;

  bool SetPublicKeysFromASCIIString(const std::string& ascii_public_key);
  bool SetDisabledFeatures(const std::string& disabled_feature_list);
  bool SetDisabledTokens(const std::string& disabled_token_list);

 private:
  std::vector<std::string> public_keys_;
  std::set<std::string> disabled_features_;
  std::set<std::string> disabled_tokens_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessOriginTrialPolicy);
};

#endif  // HEADLESS_LIB_HEADLESS_ORIGIN_TRIAL_POLICY_H_
