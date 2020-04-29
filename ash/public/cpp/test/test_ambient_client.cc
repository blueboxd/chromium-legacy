// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_ambient_client.h"

#include <utility>

#include "base/time/time.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

const char* kTestGaiaId = "0123456789";

constexpr base::TimeDelta kDefaultTokenExpirationDelay =
    base::TimeDelta::FromHours(1);

}  // namespace

TestAmbientClient::TestAmbientClient() = default;

TestAmbientClient::~TestAmbientClient() = default;

bool TestAmbientClient::IsAmbientModeAllowedForActiveUser() {
  return true;
}

void TestAmbientClient::RequestAccessToken(GetAccessTokenCallback callback) {
  pending_callback_ = std::move(callback);
}

scoped_refptr<network::SharedURLLoaderFactory>
TestAmbientClient::GetURLLoaderFactory() {
  // TODO: return fake URL loader facotry.
  return nullptr;
}

void TestAmbientClient::IssueAccessToken(const std::string& access_token,
                                         bool with_error) {
  if (!pending_callback_)
    return;

  if (with_error) {
    std::move(pending_callback_)
        .Run(/*gaia_id=*/std::string(),
             /*access_token=*/std::string(),
             /*expiration_time=*/base::Time::Now());
  } else {
    std::move(pending_callback_)
        .Run(kTestGaiaId, access_token,
             base::Time::Now() + kDefaultTokenExpirationDelay);
  }
}

bool TestAmbientClient::IsAccessTokenRequestPending() const {
  return !!pending_callback_;
}

}  // namespace ash
