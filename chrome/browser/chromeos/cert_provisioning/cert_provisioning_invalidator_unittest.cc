// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_invalidator.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace cert_provisioning {

class CertProvisioningInvalidatorTest
    : public testing::TestWithParam<CertScope> {
 protected:
  CertProvisioningInvalidatorTest()
      : kInvalidatorTopic("abcdef"),
        kSomeOtherTopic("fedcba"),
        invalidator_(CertProvisioningInvalidator::BuildAndRegister(
            GetScope(),
            &invalidation_service_,
            kInvalidatorTopic,
            base::Bind(&CertProvisioningInvalidatorTest::OnIncomingInvalidation,
                       base::Unretained(this)))) {
    EXPECT_NE(nullptr, invalidator_);

    EnableInvalidationService();
  }

  CertScope GetScope() const { return GetParam(); }

  const char* GetExpectedOwnerName() const {
    switch (GetScope()) {
      case CertScope::kUser:
        return "cert.user.abcdef";
      case CertScope::kDevice:
        return "cert.device.abcdef";
    }
  }

  void EnableInvalidationService() {
    invalidation_service_.SetInvalidatorState(syncer::INVALIDATIONS_ENABLED);
  }

  void DisableInvalidationService() {
    invalidation_service_.SetInvalidatorState(
        syncer::TRANSIENT_INVALIDATION_ERROR);
  }

  syncer::Invalidation CreateInvalidation(const syncer::Topic& topic) {
    return syncer::Invalidation::InitUnknownVersion(topic);
  }

  syncer::Invalidation FireInvalidation(const syncer::Topic& topic) {
    const syncer::Invalidation invalidation = CreateInvalidation(topic);
    invalidation_service_.EmitInvalidationForTest(invalidation);
    base::RunLoop().RunUntilIdle();
    return invalidation;
  }

  bool IsInvalidationSent(const syncer::Invalidation& invalidation) {
    return !invalidation_service_.GetMockAckHandler()->IsUnsent(invalidation);
  }

  bool IsInvalidationAcknowledged(const syncer::Invalidation& invalidation) {
    return invalidation_service_.GetMockAckHandler()->IsAcknowledged(
        invalidation);
  }

  bool IsInvalidatorRegistered(CertProvisioningInvalidator* invalidator) const {
    return !invalidation_service_.invalidator_registrar()
                .GetRegisteredTopics(invalidator)
                .empty();
  }

  bool IsInvalidatorRegistered() const {
    return IsInvalidatorRegistered(invalidator_.get());
  }

  void OnIncomingInvalidation() { ++incoming_invalidations_count_; }

  CertProvisioningInvalidatorTest(const CertProvisioningInvalidatorTest&) =
      delete;
  CertProvisioningInvalidatorTest& operator=(
      const CertProvisioningInvalidatorTest&) = delete;

  base::test::SingleThreadTaskEnvironment task_environment_;

  const syncer::Topic kInvalidatorTopic;
  const syncer::Topic kSomeOtherTopic;

  invalidation::FakeInvalidationService invalidation_service_;

  int incoming_invalidations_count_{0};

  std::unique_ptr<CertProvisioningInvalidator> invalidator_;
};

TEST_P(CertProvisioningInvalidatorTest,
       SecondInvalidatorForSameTopicCannotBeBuilt) {
  EXPECT_NE(nullptr, invalidator_);

  std::unique_ptr<CertProvisioningInvalidator> second_invalidator =
      CertProvisioningInvalidator::BuildAndRegister(
          GetScope(), &invalidation_service_, kInvalidatorTopic,
          base::DoNothing());

  EXPECT_EQ(nullptr, second_invalidator);
}

TEST_P(CertProvisioningInvalidatorTest,
       ConstructorShouldNotRegisterInvalidator) {
  EXPECT_NE(nullptr, invalidator_);

  CertProvisioningInvalidator second_invalidator(
      GetScope(), &invalidation_service_, kSomeOtherTopic, base::DoNothing());

  EXPECT_FALSE(IsInvalidatorRegistered(&second_invalidator));
}

TEST_P(CertProvisioningInvalidatorTest,
       ShouldReceiveInvalidationForRegisteredTopic) {
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_EQ(0, incoming_invalidations_count_);

  const auto invalidation = FireInvalidation(kInvalidatorTopic);

  EXPECT_TRUE(IsInvalidationSent(invalidation));
  EXPECT_TRUE(IsInvalidationAcknowledged(invalidation));
  EXPECT_EQ(1, incoming_invalidations_count_);
}

TEST_P(CertProvisioningInvalidatorTest,
       ShouldNotReceiveInvalidationForDifferentTopic) {
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_EQ(0, incoming_invalidations_count_);

  const auto invalidation = FireInvalidation(kSomeOtherTopic);

  EXPECT_FALSE(IsInvalidationSent(invalidation));
  EXPECT_FALSE(IsInvalidationAcknowledged(invalidation));
  EXPECT_EQ(0, incoming_invalidations_count_);
}

TEST_P(CertProvisioningInvalidatorTest,
       ShouldNotReceiveInvalidationWhenUnregistered) {
  EXPECT_TRUE(IsInvalidatorRegistered());

  invalidator_->Unregister();

  EXPECT_FALSE(IsInvalidatorRegistered());

  const auto invalidation = FireInvalidation(kInvalidatorTopic);

  EXPECT_FALSE(IsInvalidationSent(invalidation));
  EXPECT_FALSE(IsInvalidationAcknowledged(invalidation));
  EXPECT_EQ(0, incoming_invalidations_count_);
}

TEST_P(CertProvisioningInvalidatorTest,
       ShouldUnregisterButKeepTopicSubscribedWhenDestroyed) {
  EXPECT_TRUE(IsInvalidatorRegistered());

  invalidator_.reset();

  // Ensure that invalidator is unregistered and incoming invalidation does not
  // cause undefined behaviour.
  EXPECT_FALSE(IsInvalidatorRegistered());
  FireInvalidation(kInvalidatorTopic);
  EXPECT_EQ(0, incoming_invalidations_count_);

  // Ensure that topic is still subscribed.
  const syncer::Topics topics =
      invalidation_service_.invalidator_registrar().GetAllSubscribedTopics();
  EXPECT_NE(topics.end(), topics.find(kInvalidatorTopic));
}

TEST_P(CertProvisioningInvalidatorTest,
       ShouldHaveUniqueOwnerNameContainingScopeAndTopic) {
  EXPECT_EQ(GetExpectedOwnerName(), invalidator_->GetOwnerName());
}

INSTANTIATE_TEST_SUITE_P(CertProvisioningInvalidatorTestInstance,
                         CertProvisioningInvalidatorTest,
                         testing::Values(CertScope::kUser, CertScope::kDevice));

}  // namespace cert_provisioning
}  // namespace chromeos

