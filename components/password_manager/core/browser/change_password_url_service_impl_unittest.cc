// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/change_password_url_service_impl.h"

#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class ChangePasswordUrlServiceTest : public testing::Test {
 public:
  ChangePasswordUrlServiceTest() = default;

  // Test the bevahiour for a given |url| and compares the result to the given
  // |expected_url| in the callback.
  void TestOverride(GURL url, GURL expected_url);

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  ChangePasswordUrlServiceImpl change_password_url_service_{
      test_shared_loader_factory_};
};

void ChangePasswordUrlServiceTest::TestOverride(GURL url, GURL expected_url) {
  change_password_url_service_.Initialize();

  base::MockCallback<password_manager::ChangePasswordUrlService::UrlCallback>
      callback;
  EXPECT_CALL(callback, Run(expected_url));
  change_password_url_service_.GetChangePasswordUrl(url::Origin::Create(url),
                                                    callback.Get());
}

TEST_F(ChangePasswordUrlServiceTest, eTLDLookup) {
  // TODO(crbug.com/1086141): If possible mock eTLD registry to ensure sites are
  // listed.
  TestOverride(GURL("https://google.com/foo"), GURL("https://google.com/"));
}

}  // namespace password_manager
