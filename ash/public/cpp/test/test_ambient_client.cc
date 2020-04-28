// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_ambient_client.h"

#include "base/callback.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

TestAmbientClient::TestAmbientClient() = default;

TestAmbientClient::~TestAmbientClient() = default;

bool TestAmbientClient::IsAmbientModeAllowedForActiveUser() {
  return true;
}

void TestAmbientClient::RequestAccessToken(GetAccessTokenCallback callback) {
  std::move(callback).Run(/*gaia_id=*/std::string(),
                          /*access_token=*/std::string());
}

scoped_refptr<network::SharedURLLoaderFactory>
TestAmbientClient::GetURLLoaderFactory() {
  // TODO: return fake URL loader facotry.
  return nullptr;
}

}  // namespace ash
