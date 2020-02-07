// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/headless_origin_trial_policy.h"

#include <stdint.h>

#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "content/public/common/origin_util.h"

// This is the default public key used for validating signatures.
static const uint8_t kDefaultPublicKey[] = {
    0x7c, 0xc4, 0xb8, 0x9a, 0x93, 0xba, 0x6e, 0xe2, 0xd0, 0xfd, 0x03,
    0x1d, 0xfb, 0x32, 0x66, 0xc7, 0x3b, 0x72, 0xfd, 0x54, 0x3a, 0x07,
    0x51, 0x14, 0x66, 0xaa, 0x02, 0x53, 0x4e, 0x33, 0xa1, 0x15,
};

// TODO(crbug.com/1049317): Move the Chrome definition of these switches into
// a shared location (see
// https://source.chromium.org/chromium/chromium/src/+/master:chrome/common/chrome_switches.h;l=137;drc=66ee8f655d42c11d34d527e42f6043db540fee79).

// Contains a list of feature names for which origin trial experiments should
// be disabled. Names should be separated by "|" characters.
const char kOriginTrialDisabledFeatures[] = "origin-trial-disabled-features";

// Contains a list of token signatures for which origin trial experiments should
// be disabled. Tokens should be separated by "|" characters.
const char kOriginTrialDisabledTokens[] = "origin-trial-disabled-tokens";

// Comma-separated list of keys which will override the default public keys for
// checking origin trial tokens.
const char kOriginTrialPublicKey[] = "origin-trial-public-key";

HeadlessOriginTrialPolicy::HeadlessOriginTrialPolicy()
    : public_keys_(1,
                   std::string(reinterpret_cast<const char*>(kDefaultPublicKey),
                               base::size(kDefaultPublicKey))) {
  // Set the public key and disabled feature list for the origin trial key
  // manager, based on the command line flags which were passed to this process.
  // If the flags are not present, or are incorrectly formatted, the defaults
  // will remain active.
  if (base::CommandLine::InitializedForCurrentProcess()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(kOriginTrialPublicKey)) {
      SetPublicKeysFromASCIIString(
          command_line->GetSwitchValueASCII(kOriginTrialPublicKey));
    }
    if (command_line->HasSwitch(kOriginTrialDisabledFeatures)) {
      SetDisabledFeatures(
          command_line->GetSwitchValueASCII(kOriginTrialDisabledFeatures));
    }
    if (command_line->HasSwitch(kOriginTrialDisabledTokens)) {
      SetDisabledTokens(
          command_line->GetSwitchValueASCII(kOriginTrialDisabledTokens));
    }
  }
}

HeadlessOriginTrialPolicy::~HeadlessOriginTrialPolicy() = default;

bool HeadlessOriginTrialPolicy::IsOriginTrialsSupported() const {
  return true;
}

std::vector<base::StringPiece> HeadlessOriginTrialPolicy::GetPublicKeys()
    const {
  std::vector<base::StringPiece> casted_public_keys;
  for (auto const& key : public_keys_) {
    casted_public_keys.push_back(base::StringPiece(key));
  }
  return casted_public_keys;
}

bool HeadlessOriginTrialPolicy::IsFeatureDisabled(
    base::StringPiece feature) const {
  return disabled_features_.count(feature.as_string()) > 0;
}

bool HeadlessOriginTrialPolicy::IsTokenDisabled(
    base::StringPiece token_signature) const {
  return disabled_tokens_.count(token_signature.as_string()) > 0;
}

bool HeadlessOriginTrialPolicy::IsOriginSecure(const GURL& url) const {
  return content::IsOriginSecure(url);
}

bool HeadlessOriginTrialPolicy::SetPublicKeysFromASCIIString(
    const std::string& ascii_public_keys) {
  std::vector<std::string> new_public_keys;
  const auto public_keys = base::SplitString(
      ascii_public_keys, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& ascii_public_key : public_keys) {
    // Base64-decode the incoming string. Set the key if it is correctly
    // formatted
    std::string new_public_key;
    if (!base::Base64Decode(ascii_public_key, &new_public_key))
      return false;
    if (new_public_key.size() != 32)
      return false;
    new_public_keys.push_back(new_public_key);
  }
  if (!new_public_keys.empty()) {
    public_keys_.swap(new_public_keys);
    return true;
  }
  return false;
}

bool HeadlessOriginTrialPolicy::SetDisabledFeatures(
    const std::string& disabled_feature_list) {
  std::set<std::string> new_disabled_features;
  const std::vector<std::string> features =
      base::SplitString(disabled_feature_list, "|", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const std::string& feature : features)
    new_disabled_features.insert(feature);
  disabled_features_.swap(new_disabled_features);
  return true;
}

bool HeadlessOriginTrialPolicy::SetDisabledTokens(
    const std::string& disabled_token_list) {
  std::set<std::string> new_disabled_tokens;
  const std::vector<std::string> tokens =
      base::SplitString(disabled_token_list, "|", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const std::string& ascii_token : tokens) {
    std::string token_signature;
    if (!base::Base64Decode(ascii_token, &token_signature))
      continue;
    if (token_signature.size() != 64)
      continue;
    new_disabled_tokens.insert(token_signature);
  }
  disabled_tokens_.swap(new_disabled_tokens);
  return true;
}
