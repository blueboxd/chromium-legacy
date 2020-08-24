// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::StorageType;

namespace storage {

namespace {

constexpr StorageType kTemporary = StorageType::kTemporary;
constexpr StorageType kPersistent = StorageType::kPersistent;

constexpr QuotaClientType kClientFile = QuotaClientType::kFileSystem;
constexpr QuotaClientType kClientDB = QuotaClientType::kIndexedDatabase;

}  // namespace

class MockQuotaManagerTest : public testing::Test {
 public:
  MockQuotaManagerTest() : deletion_callback_count_(0) {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    policy_ = new MockSpecialStoragePolicy;
    manager_ = new MockQuotaManager(
        false /* is_incognito */, data_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get().get(), policy_.get());
  }

  void TearDown() override {
    // Make sure the quota manager cleans up correctly.
    manager_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  void GetModifiedOrigins(StorageType type, base::Time begin, base::Time end) {
    manager_->GetOriginsModifiedBetween(
        type, begin, end,
        base::BindOnce(&MockQuotaManagerTest::GotModifiedOrigins,
                       weak_factory_.GetWeakPtr()));
  }

  void GotModifiedOrigins(const std::set<url::Origin>& origins,
                          StorageType type) {
    origins_ = origins;
    type_ = type;
  }

  void DeleteOriginData(const url::Origin& origin,
                        StorageType type,
                        QuotaClientTypes quota_client_types) {
    manager_->DeleteOriginData(
        origin, type, std::move(quota_client_types),
        base::BindOnce(&MockQuotaManagerTest::DeletedOriginData,
                       weak_factory_.GetWeakPtr()));
  }

  void DeletedOriginData(blink::mojom::QuotaStatusCode status) {
    ++deletion_callback_count_;
    EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status);
  }

  int deletion_callback_count() const {
    return deletion_callback_count_;
  }

  MockQuotaManager* manager() const {
    return manager_.get();
  }

  const std::set<url::Origin>& origins() const { return origins_; }

  const StorageType& type() const {
    return type_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<MockQuotaManager> manager_;
  scoped_refptr<MockSpecialStoragePolicy> policy_;

  int deletion_callback_count_;

  std::set<url::Origin> origins_;
  StorageType type_;

  base::WeakPtrFactory<MockQuotaManagerTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManagerTest);
};

TEST_F(MockQuotaManagerTest, BasicOriginManipulation) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));

  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kTemporary, kClientDB));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kPersistent, kClientDB));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kTemporary, kClientDB));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kPersistent, kClientDB));

  manager()->AddOrigin(kOrigin1, kTemporary, {kClientFile}, base::Time::Now());
  EXPECT_TRUE(manager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kTemporary, kClientDB));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kPersistent, kClientDB));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kTemporary, kClientDB));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kPersistent, kClientDB));

  manager()->AddOrigin(kOrigin1, kPersistent, {kClientFile}, base::Time::Now());
  EXPECT_TRUE(manager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kTemporary, kClientDB));
  EXPECT_TRUE(manager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kPersistent, kClientDB));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kTemporary, kClientDB));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kPersistent, kClientDB));

  manager()->AddOrigin(kOrigin2, kTemporary, {kClientFile, kClientDB},
                       base::Time::Now());
  EXPECT_TRUE(manager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kTemporary, kClientDB));
  EXPECT_TRUE(manager()->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin1, kPersistent, kClientDB));
  EXPECT_TRUE(manager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_TRUE(manager()->OriginHasData(kOrigin2, kTemporary, kClientDB));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kPersistent, kClientDB));
}

TEST_F(MockQuotaManagerTest, OriginDeletion) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1/"));

  manager()->AddOrigin(kOrigin1, kTemporary, {kClientFile}, base::Time::Now());
  manager()->AddOrigin(kOrigin2, kTemporary, {kClientFile, kClientDB},
                       base::Time::Now());
  manager()->AddOrigin(kOrigin3, kTemporary, {kClientFile, kClientDB},
                       base::Time::Now());

  DeleteOriginData(kOrigin2, kTemporary, {kClientFile});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, deletion_callback_count());
  EXPECT_TRUE(manager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_TRUE(manager()->OriginHasData(kOrigin2, kTemporary, kClientDB));
  EXPECT_TRUE(manager()->OriginHasData(kOrigin3, kTemporary, kClientFile));
  EXPECT_TRUE(manager()->OriginHasData(kOrigin3, kTemporary, kClientDB));

  DeleteOriginData(kOrigin3, kTemporary, {kClientFile, kClientDB});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, deletion_callback_count());
  EXPECT_TRUE(manager()->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_TRUE(manager()->OriginHasData(kOrigin2, kTemporary, kClientDB));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin3, kTemporary, kClientFile));
  EXPECT_FALSE(manager()->OriginHasData(kOrigin3, kTemporary, kClientDB));
}

TEST_F(MockQuotaManagerTest, ModifiedOrigins) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1/"));

  base::Time now = base::Time::Now();
  base::Time then = base::Time();
  base::TimeDelta an_hour = base::TimeDelta::FromMilliseconds(3600000);
  base::TimeDelta a_minute = base::TimeDelta::FromMilliseconds(60000);

  GetModifiedOrigins(kTemporary, then, base::Time::Max());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(origins().empty());

  manager()->AddOrigin(kOrigin1, kTemporary, {kClientFile}, now - an_hour);

  GetModifiedOrigins(kTemporary, then, base::Time::Max());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, origins().size());
  EXPECT_EQ(1UL, origins().count(kOrigin1));
  EXPECT_EQ(0UL, origins().count(kOrigin2));

  manager()->AddOrigin(kOrigin2, kTemporary, {kClientFile}, now);

  GetModifiedOrigins(kTemporary, then, base::Time::Max());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(2UL, origins().size());
  EXPECT_EQ(1UL, origins().count(kOrigin1));
  EXPECT_EQ(1UL, origins().count(kOrigin2));

  GetModifiedOrigins(kTemporary, then, now);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, origins().size());
  EXPECT_EQ(1UL, origins().count(kOrigin1));
  EXPECT_EQ(0UL, origins().count(kOrigin2));

  GetModifiedOrigins(kTemporary, now - a_minute, now + a_minute);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, origins().size());
  EXPECT_EQ(0UL, origins().count(kOrigin1));
  EXPECT_EQ(1UL, origins().count(kOrigin2));
}
}  // namespace storage
