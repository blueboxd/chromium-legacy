// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_AMBIENT_CLIENT_H_
#define ASH_PUBLIC_CPP_TEST_TEST_AMBIENT_CLIENT_H_

#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// An implementation for test support.
// IsAmbientModeAllowedForProfile() returns true to run some unittests.
class ASH_PUBLIC_EXPORT TestAmbientClient : public AmbientClient {
 public:
  TestAmbientClient();
  ~TestAmbientClient() override;

  // AmbientClient:
  bool IsAmbientModeAllowedForActiveUser() override;
  void RequestAccessToken(GetAccessTokenCallback callback) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_AMBIENT_CLIENT_H_
