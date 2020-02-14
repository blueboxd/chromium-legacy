// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"

#include "base/strings/string_piece_forward.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

struct MockSavedPasswordsPresenter : SavedPasswordsPresenter {
  MOCK_METHOD(void,
              EditPassword,
              (const autofill::PasswordForm&, base::StringPiece16),
              (override));

  MOCK_METHOD(std::vector<autofill::PasswordForm>,
              GetSavedPasswords,
              (),
              (override));
};

class BulkLeakCheckServiceAdapterTest : public ::testing::Test {
 public:
  BulkLeakCheckServiceAdapterTest()
      : service_(identity_test_env_.identity_manager(),
                 base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {
    service_.set_leak_factory(
        std::make_unique<MockLeakDetectionCheckFactory>());
  }

  BulkLeakCheckServiceAdapter& adapter() { return adapter_; }

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  ::testing::StrictMock<MockSavedPasswordsPresenter> presenter_;
  BulkLeakCheckService service_;
  BulkLeakCheckServiceAdapter adapter_{&presenter_, &service_};
};

}  // namespace

TEST_F(BulkLeakCheckServiceAdapterTest, OnCreation) {
  EXPECT_EQ(0u, adapter().GetPendingChecksCount());
  EXPECT_EQ(BulkLeakCheckService::State::kIdle,
            adapter().GetBulkLeakCheckState());
}

}  // namespace password_manager
