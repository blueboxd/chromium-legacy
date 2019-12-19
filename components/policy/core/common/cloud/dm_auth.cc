// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/dm_auth.h"

#include "base/memory/ptr_util.h"

namespace policy {

DMAuth::DMAuth() = default;
DMAuth::~DMAuth() = default;

// static
std::unique_ptr<DMAuth> DMAuth::FromDMToken(const std::string& dm_token) {
  return base::WrapUnique(new DMAuth(dm_token, DMAuthTokenType::kDm));
}

// static
std::unique_ptr<DMAuth> DMAuth::FromGaiaToken(const std::string& gaia_token) {
  return base::WrapUnique(new DMAuth(gaia_token, DMAuthTokenType::kGaia));
}

// static
std::unique_ptr<DMAuth> DMAuth::FromOAuthToken(const std::string& oauth_token) {
  return base::WrapUnique(new DMAuth(oauth_token, DMAuthTokenType::kOauth));
}

// static
std::unique_ptr<DMAuth> DMAuth::FromEnrollmentToken(
    const std::string& enrollment_token) {
  return base::WrapUnique(
      new DMAuth(enrollment_token, DMAuthTokenType::kEnrollment));
}

// static
std::unique_ptr<DMAuth> DMAuth::NoAuth() {
  return std::make_unique<DMAuth>();
}

DMAuth::DMAuth(const std::string& token, DMAuthTokenType token_type)
    : token_(token), token_type_(token_type) {}

std::unique_ptr<DMAuth> DMAuth::Clone() const {
  std::unique_ptr<DMAuth> result = std::make_unique<DMAuth>();
  *result = *this;
  return result;
}

bool DMAuth::Equals(const DMAuth& other) const {
  return token_ == other.token_ && token_type_ == other.token_type_;
}

}  // namespace policy
