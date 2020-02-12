// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_provider.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

struct MockCompromisedCredentialsProviderObserver
    : ::testing::StrictMock<CompromisedCredentialsProvider::Observer> {
  MOCK_METHOD1(OnCompromisedCredentialsChanged,
               void(CompromisedCredentialsProvider::CredentialsView));
};

class CompromisedCredentialsProviderTest : public ::testing::Test {
 protected:
  CompromisedCredentialsProvider& provider() { return provider_; }

 private:
  CompromisedCredentialsProvider provider_;
};

}  // namespace

// Tests whether adding and removing an observer works as expected.
TEST_F(CompromisedCredentialsProviderTest, Observers) {
  MockCompromisedCredentialsProviderObserver observer;
  provider().AddObserver(&observer);
  provider().RemoveObserver(&observer);
}

}  // namespace password_manager
