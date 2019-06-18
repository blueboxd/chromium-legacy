// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using blink::IndexedDBDatabaseMetadata;
using url::Origin;

namespace content {
namespace {

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

void CreateAndBindTransactionPlaceholder(
    base::WeakPtr<IndexedDBTransaction> transaction) {}

class LevelDBLock {
 public:
  LevelDBLock() : env_(nullptr), lock_(nullptr) {}
  LevelDBLock(leveldb::Env* env, leveldb::FileLock* lock)
      : env_(env), lock_(lock) {}
  ~LevelDBLock() {
    if (env_)
      env_->UnlockFile(lock_);
  }

 private:
  leveldb::Env* env_;
  leveldb::FileLock* lock_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBLock);
};

std::unique_ptr<LevelDBLock> LockForTesting(const base::FilePath& file_name) {
  leveldb::Env* env = LevelDBEnv::Get();
  base::FilePath lock_path = file_name.AppendASCII("LOCK");
  leveldb::FileLock* lock = nullptr;
  leveldb::Status status = env->LockFile(lock_path.AsUTF8Unsafe(), &lock);
  if (!status.ok())
    return std::unique_ptr<LevelDBLock>();
  DCHECK(lock);
  return std::make_unique<LevelDBLock>(env, lock);
}

}  // namespace

class IndexedDBTest : public testing::Test {
 public:
  const Origin kNormalOrigin;
  const Origin kSessionOnlyOrigin;

  IndexedDBTest()
      : kNormalOrigin(url::Origin::Create(GURL("http://normal/"))),
        kSessionOnlyOrigin(url::Origin::Create(GURL("http://session-only/"))),
        special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        quota_manager_proxy_(
            base::MakeRefCounted<MockQuotaManagerProxy>(nullptr, nullptr)),
        context_(base::MakeRefCounted<IndexedDBContextImpl>(
            CreateAndReturnTempDir(&temp_dir_),
            /*special_storage_policy=*/special_storage_policy_.get(),
            quota_manager_proxy_.get(),
            base::DefaultClock::GetInstance())) {
    special_storage_policy_->AddSessionOnly(kSessionOnlyOrigin.GetURL());
  }
  ~IndexedDBTest() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  }

 protected:
  IndexedDBContextImpl* context() const { return context_.get(); }
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;

 private:
  TestBrowserThreadBundle thread_bundle_;
  TestBrowserContext browser_context_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<IndexedDBContextImpl> context_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBTest);
};

TEST_F(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::FilePath normal_path;
  base::FilePath session_only_path;

  normal_path = context()->GetFilePathForTesting(kNormalOrigin);
  session_only_path = context()->GetFilePathForTesting(kSessionOnlyOrigin);
  ASSERT_TRUE(base::CreateDirectory(normal_path));
  ASSERT_TRUE(base::CreateDirectory(session_only_path));
  RunAllTasksUntilIdle();
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();

  RunAllTasksUntilIdle();

  context()->Shutdown();

  RunAllTasksUntilIdle();

  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_FALSE(base::DirectoryExists(session_only_path));
}

TEST_F(IndexedDBTest, SetForceKeepSessionState) {
  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Save session state. This should bypass the destruction-time deletion.
  context()->SetForceKeepSessionState();

  normal_path = context()->GetFilePathForTesting(kNormalOrigin);
  session_only_path = context()->GetFilePathForTesting(kSessionOnlyOrigin);
  ASSERT_TRUE(base::CreateDirectory(normal_path));
  ASSERT_TRUE(base::CreateDirectory(session_only_path));
  base::RunLoop().RunUntilIdle();

  context()->Shutdown();

  base::RunLoop().RunUntilIdle();

  // No data was cleared because of SetForceKeepSessionState.
  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_TRUE(base::DirectoryExists(session_only_path));
}

class ForceCloseDBCallbacks : public IndexedDBCallbacks {
 public:
  ForceCloseDBCallbacks(scoped_refptr<IndexedDBContextImpl> idb_context,
                        const Origin& origin)
      : IndexedDBCallbacks(nullptr, origin, nullptr, idb_context->TaskRunner()),
        idb_context_(idb_context),
        origin_(origin) {}

  void OnSuccess() override {}
  void OnSuccess(const std::vector<base::string16>&) override {}
  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const IndexedDBDatabaseMetadata& metadata) override {
    connection_ = std::move(connection);
    idb_context_->ConnectionOpened(origin_, connection_.get());
  }

  IndexedDBConnection* connection() { return connection_.get(); }

 protected:
  ~ForceCloseDBCallbacks() override {}

 private:
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  Origin origin_;
  std::unique_ptr<IndexedDBConnection> connection_;
  DISALLOW_COPY_AND_ASSIGN(ForceCloseDBCallbacks);
};

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnDelete) {
  const Origin kTestOrigin = Origin::Create(GURL("http://test/"));

  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const int child_process_id = 0;
        const int64_t host_transaction_id = 0;
        const int64_t version = 0;

        IndexedDBFactory* factory = context()->GetIDBFactory();

        base::FilePath test_path =
            context()->GetFilePathForTesting(kTestOrigin);

        auto open_db_callbacks =
            base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
        auto closed_db_callbacks =
            base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
        auto open_callbacks =
            base::MakeRefCounted<ForceCloseDBCallbacks>(context(), kTestOrigin);
        auto closed_callbacks =
            base::MakeRefCounted<ForceCloseDBCallbacks>(context(), kTestOrigin);

        auto create_transaction_callback1 =
            base::BindOnce(&CreateAndBindTransactionPlaceholder);
        factory->Open(base::ASCIIToUTF16("opendb"),
                      std::make_unique<IndexedDBPendingConnection>(
                          open_callbacks, open_db_callbacks, child_process_id,
                          host_transaction_id, version,
                          std::move(create_transaction_callback1)),
                      kTestOrigin, context()->data_path());
        EXPECT_TRUE(base::DirectoryExists(test_path));

        auto create_transaction_callback2 =
            base::BindOnce(&CreateAndBindTransactionPlaceholder);
        factory->Open(base::ASCIIToUTF16("closeddb"),
                      std::make_unique<IndexedDBPendingConnection>(
                          closed_callbacks, closed_db_callbacks,
                          child_process_id, host_transaction_id, version,
                          std::move(create_transaction_callback2)),
                      kTestOrigin, context()->data_path());

        closed_callbacks->connection()->Close();

        context()->DeleteForOrigin(kTestOrigin);

        EXPECT_TRUE(open_db_callbacks->forced_close_called());
        EXPECT_FALSE(closed_db_callbacks->forced_close_called());
        EXPECT_FALSE(base::DirectoryExists(test_path));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBTest, DeleteFailsIfDirectoryLocked) {
  const Origin kTestOrigin = Origin::Create(GURL("http://test/"));

  base::FilePath test_path = context()->GetFilePathForTesting(kTestOrigin);
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto lock = LockForTesting(test_path);
  ASSERT_TRUE(lock);

  base::RunLoop loop;
  context()->TaskRunner()->PostTask(FROM_HERE,
                                    base::BindLambdaForTesting([&]() {
                                      context()->DeleteForOrigin(kTestOrigin);
                                      loop.Quit();
                                    }));
  loop.Run();

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailure) {
  base::RunLoop loop;
  context()->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const Origin kTestOrigin = Origin::Create(GURL("http://test/"));

        auto* factory =
            static_cast<IndexedDBFactoryImpl*>(context()->GetIDBFactory());

        const int child_process_id = 0;
        const int64_t transaction_id = 1;

        auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
        auto db_callbacks =
            base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
        auto create_transaction_callback1 =
            base::BindOnce(&CreateAndBindTransactionPlaceholder);
        auto connection = std::make_unique<IndexedDBPendingConnection>(
            callbacks, db_callbacks, child_process_id, transaction_id,
            IndexedDBDatabaseMetadata::DEFAULT_VERSION,
            std::move(create_transaction_callback1));
        factory->Open(base::ASCIIToUTF16("db"), std::move(connection),
                      Origin(kTestOrigin), context()->data_path());

        EXPECT_TRUE(callbacks->connection());

        // ConnectionOpened() is usually called by the dispatcher.
        context()->ConnectionOpened(kTestOrigin, callbacks->connection());

        EXPECT_TRUE(factory->IsBackingStoreOpen(kTestOrigin));

        // Simulate the write failure.
        leveldb::Status status = leveldb::Status::IOError("Simulated failure");
        factory->HandleBackingStoreFailure(kTestOrigin);

        EXPECT_TRUE(db_callbacks->forced_close_called());
        EXPECT_FALSE(factory->IsBackingStoreOpen(kTestOrigin));

        loop.Quit();
      }));
  loop.Run();
}

}  // namespace content
