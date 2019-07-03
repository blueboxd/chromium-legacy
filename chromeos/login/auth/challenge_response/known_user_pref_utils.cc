// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/challenge_response/known_user_pref_utils.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/values.h"

namespace chromeos {

namespace {

constexpr char kPublicKeySpkiKey[] = "public_key_spki";

}  // namespace

base::Value SerializeChallengeResponseKeysForKnownUser(
    const std::vector<ChallengeResponseKey>& challenge_response_keys) {
  base::Value pref_value(base::Value::Type::LIST);
  for (const auto& key : challenge_response_keys) {
    std::string spki_base64;
    base::Base64Encode(key.public_key_spki_der(), &spki_base64);
    base::Value key_representation(base::Value::Type::DICTIONARY);
    key_representation.SetKey(kPublicKeySpkiKey, base::Value(spki_base64));
    pref_value.GetList().emplace_back(std::move(key_representation));
  }
  return pref_value;
}

}  // namespace chromeos
