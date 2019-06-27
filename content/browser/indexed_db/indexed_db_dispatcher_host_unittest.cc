// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/default_clock.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"
#include "url/origin.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;
using blink::IndexedDBKey;
using blink::mojom::IDBCallbacks;
using blink::mojom::IDBCallbacksAssociatedPtrInfo;
using blink::mojom::IDBDatabaseAssociatedPtr;
using blink::mojom::IDBDatabaseAssociatedPtrInfo;
using blink::mojom::IDBDatabaseAssociatedRequest;
using blink::mojom::IDBDatabaseCallbacks;
using blink::mojom::IDBDatabaseCallbacksAssociatedPtrInfo;
using blink::mojom::IDBFactory;
using blink::mojom::IDBFactoryPtr;
using blink::mojom::IDBValue;
using blink::mojom::IDBValuePtr;
using mojo::StrongAssociatedBindingPtr;
using testing::_;
using testing::StrictMock;

namespace content {
namespace {

// TODO(crbug.com/889590): Replace with common converter.
url::Origin ToOrigin(const std::string& url) {
  return url::Origin::Create(GURL(url));
}

ACTION_TEMPLATE(MoveArg,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = std::move(*::testing::get<k>(args));
}

ACTION_P(RunClosure, closure) {
  closure.Run();
}

ACTION_P(QuitLoop, run_loop) {
  run_loop->Quit();
}

MATCHER_P(IsAssociatedInterfacePtrInfoValid,
          tf,
          std::string(negation ? "isn't" : "is") + " " +
              std::string(tf ? "valid" : "invalid")) {
  return tf == arg->is_valid();
}

MATCHER_P(MatchesIDBKey, key, "") {
  return arg.Equals(key);
}

typedef void (base::Closure::*ClosureRunFcn)() const &;

static const char kDatabaseName[] = "db";
static const char kOrigin[] = "https://www.example.com";
static const int kFakeProcessId = 2;

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

// Stores data specific to a connection.
struct TestDatabaseConnection {
  TestDatabaseConnection() = default;

  TestDatabaseConnection(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         url::Origin origin,
                         base::string16 db_name,
                         int64_t version,
                         int64_t upgrade_txn_id)
      : task_runner(std::move(task_runner)),
        origin(std::move(origin)),
        db_name(std::move(db_name)),
        version(version),
        upgrade_txn_id(upgrade_txn_id),
        open_callbacks(new StrictMock<MockMojoIndexedDBCallbacks>()),
        connection_callbacks(
            new StrictMock<MockMojoIndexedDBDatabaseCallbacks>()) {}
  TestDatabaseConnection& operator=(TestDatabaseConnection&& other) noexcept =
      default;
  ~TestDatabaseConnection() {}

  void Open(IDBFactory* factory) {
    factory->Open(open_callbacks->CreateInterfacePtrAndBind(),
                  connection_callbacks->CreateInterfacePtrAndBind(), db_name,
                  version,
                  mojo::MakeRequest(&version_change_transaction, task_runner),
                  upgrade_txn_id);
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner;
  url::Origin origin;
  base::string16 db_name;
  int64_t version;
  int64_t upgrade_txn_id;

  IDBDatabaseAssociatedPtr database;
  blink::mojom::IDBTransactionAssociatedPtr version_change_transaction;

  std::unique_ptr<MockMojoIndexedDBCallbacks> open_callbacks;
  std::unique_ptr<MockMojoIndexedDBDatabaseCallbacks> connection_callbacks;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDatabaseConnection);
};

void StatusCallback(const base::Closure& callback,
                    blink::mojom::IDBStatus* status_out,
                    blink::mojom::IDBStatus status) {
  *status_out = status;
  callback.Run();
}

class TestIndexedDBObserver : public IndexedDBContextImpl::Observer {
 public:
  void OnIndexedDBListChanged(const url::Origin& origin) override {
    ++notify_list_changed_count;
  }

  void OnIndexedDBContentChanged(
      const url::Origin& origin,
      const base::string16& database_name,
      const base::string16& object_store_name) override {
    ++notify_content_changed_count;
  }

  int notify_list_changed_count = 0;
  int notify_content_changed_count = 0;
};

}  // namespace

class IndexedDBDispatcherHostTest : public testing::Test {
 public:
  IndexedDBDispatcherHostTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        quota_manager_(base::MakeRefCounted<MockQuotaManager>(
            false /*is_incognito*/,
            browser_context_.GetPath(),
            base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
            special_storage_policy_)),
        context_impl_(base::MakeRefCounted<IndexedDBContextImpl>(
            CreateAndReturnTempDir(&temp_dir_),
            special_storage_policy_,
            quota_manager_->proxy(),
            base::DefaultClock::GetInstance())),
        host_(new IndexedDBDispatcherHost(
                  kFakeProcessId,
                  context_impl_,
                  ChromeBlobStorageContext::GetFor(&browser_context_)),
              base::OnTaskRunnerDeleter(context_impl_->TaskRunner())) {}

  void TearDown() override {
    base::RunLoop loop;
    context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                          base::BindLambdaForTesting([&]() {
                                            idb_mojo_factory_ = nullptr;
                                            loop.Quit();
                                          }));
    loop.Run();
    host_.reset();
    context_impl_ = nullptr;
    quota_manager_ = nullptr;
    RunAllTasksUntilIdle();
    // File are leaked if this doesn't return true.
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void SetUp() override {
    base::RunLoop loop;
    context_impl_->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          host_->AddBinding(::mojo::MakeRequest(&idb_mojo_factory_),
                            {url::Origin::Create(GURL(kOrigin))});
          loop.Quit();
        }));
    loop.Run();
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  TestBrowserContext browser_context_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<MockQuotaManager> quota_manager_;
  scoped_refptr<IndexedDBContextImpl> context_impl_;
  std::unique_ptr<IndexedDBDispatcherHost, base::OnTaskRunnerDeleter> host_;
  IDBFactoryPtr idb_mojo_factory_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcherHostTest);
};

TEST_F(IndexedDBDispatcherHostTest, CloseConnectionBeforeUpgrade) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  IDBDatabaseAssociatedPtrInfo database_info;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), url::Origin::Create(GURL(kOrigin)),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);
        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(""), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  base::RunLoop loop2;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          connection.reset();
                                          loop2.Quit();
                                        }));
  loop2.Run();
}

TEST_F(IndexedDBDispatcherHostTest, CloseAfterUpgrade) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  IDBDatabaseAssociatedPtrInfo database_info;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);

        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(""), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  ASSERT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(2, loop2.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;
        EXPECT_CALL(*connection->connection_callbacks, Complete(kTransactionId))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(std::move(quit_closure2)));

        connection->database.Bind(std::move(database_info));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        connection->version_change_transaction->CreateObjectStore(
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);
        connection->version_change_transaction->Commit(0);
      }));
  loop2.Run();

  base::RunLoop loop3;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          connection.reset();
                                          loop3.Quit();
                                        }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest, OpenNewConnectionWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";
  std::unique_ptr<TestDatabaseConnection> connection1;
  IDBDatabaseAssociatedPtrInfo database_info1;
  IndexedDBDatabaseMetadata metadata1;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection 1, and expect the upgrade needed.
        connection1 = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), url::Origin::Create(GURL(kOrigin)),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);

        EXPECT_CALL(*connection1->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(""), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info1),
                                     testing::SaveArg<4>(&metadata1),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection1->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  std::unique_ptr<TestDatabaseConnection> connection2;
  IDBDatabaseAssociatedPtrInfo database_info2;
  IndexedDBDatabaseMetadata metadata2;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        connection2 = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, 0);

        // Check that we're called in order and the second connection gets it's
        // database after the first connection completes.
        ::testing::InSequence dummy;
        EXPECT_CALL(*connection1->connection_callbacks,
                    Complete(kTransactionId))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection1->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection2->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(true), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info2),
                                     testing::SaveArg<1>(&metadata2),
                                     RunClosure(std::move(quit_closure2))));

        connection1->database.Bind(std::move(database_info1));
        ASSERT_TRUE(connection1->database.is_bound());
        ASSERT_TRUE(connection1->version_change_transaction.is_bound());

        // Open connection 2, but expect that we won't be called back.
        connection2->Open(idb_mojo_factory_.get());

        // Create object store.
        connection1->version_change_transaction->CreateObjectStore(
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);
        connection1->version_change_transaction->Commit(0);
      }));
  loop2.Run();

  EXPECT_TRUE(database_info2.is_valid());
  EXPECT_EQ(connection2->version, metadata2.version);
  EXPECT_EQ(connection2->db_name, metadata2.name);

  base::RunLoop loop3;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          connection1.reset();
                                          connection2.reset();
                                          loop3.Quit();
                                        }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest, PutWithInvalidBlob) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  IDBDatabaseAssociatedPtrInfo database_info;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), url::Origin::Create(GURL(kOrigin)),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);

        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(""), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  ASSERT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  std::unique_ptr<StrictMock<MockMojoIndexedDBCallbacks>> put_callbacks;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        put_callbacks =
            std::make_unique<StrictMock<MockMojoIndexedDBCallbacks>>();

        EXPECT_CALL(*put_callbacks,
                    Error(blink::kWebIDBDatabaseExceptionUnknownError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        EXPECT_CALL(*connection->connection_callbacks,
                    Abort(kTransactionId,
                          blink::kWebIDBDatabaseExceptionUnknownError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        EXPECT_CALL(*connection->open_callbacks,
                    Error(blink::kWebIDBDatabaseExceptionAbortError, _))
            .Times(1)
            .WillOnce(RunClosure(std::move(quit_closure2)));

        connection->database.Bind(std::move(database_info));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        connection->version_change_transaction->CreateObjectStore(
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);
        // Call Put with an invalid blob.
        std::vector<blink::mojom::IDBBlobInfoPtr> blobs;
        blink::mojom::BlobPtrInfo blob;
        // Ignore the result of MakeRequest, to end up with an invalid blob.
        mojo::MakeRequest(&blob);
        blobs.push_back(blink::mojom::IDBBlobInfo::New(
            std::move(blob), "fakeUUID", base::string16(), 100, nullptr));

        std::string value = "hello";
        const char* value_data = value.data();
        std::vector<uint8_t> value_vector(value_data,
                                          value_data + value.length());

        auto new_value = blink::mojom::IDBValue::New();
        new_value->bits = std::move(value_vector);
        new_value->blob_or_file_info = std::move(blobs);

        connection->version_change_transaction->Put(
            kObjectStoreId, std::move(new_value),
            IndexedDBKey(base::UTF8ToUTF16("hello")),
            blink::mojom::IDBPutMode::AddOnly,
            std::vector<IndexedDBIndexKeys>(),
            put_callbacks->CreateInterfacePtrAndBind());
        connection->version_change_transaction->Commit(0);
      }));
  loop2.Run();

  base::RunLoop loop3;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          put_callbacks.reset();
                                          connection.reset();
                                          loop3.Quit();
                                        }));
  loop3.Run();
}

// Disabled for crbug.com/945627.
TEST_F(IndexedDBDispatcherHostTest, DISABLED_CompactDatabaseWithConnection) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  IDBDatabaseAssociatedPtrInfo database_info;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);
        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(*connection->connection_callbacks, Complete(kTransactionId))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(database_info));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());

        connection->version_change_transaction->Commit(0);
        idb_mojo_factory_->AbortTransactionsAndCompactDatabase(base::BindOnce(
            &StatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();
  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          connection.reset();
                                          loop3.Quit();
                                        }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest, CompactDatabaseWhileDoingTransaction) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  IDBDatabaseAssociatedPtrInfo database_info;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);
        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(4, loop2.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(*connection->connection_callbacks,
                    Abort(kTransactionId,
                          blink::kWebIDBDatabaseExceptionUnknownError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->open_callbacks,
                    Error(blink::kWebIDBDatabaseExceptionAbortError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->connection_callbacks, ForcedClose())
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(database_info));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        connection->version_change_transaction->CreateObjectStore(
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);
        idb_mojo_factory_->AbortTransactionsAndCompactDatabase(base::BindOnce(
            &StatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();

  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          connection.reset();
                                          loop3.Quit();
                                        }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest, CompactDatabaseWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  IDBDatabaseAssociatedPtrInfo database_info;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);
        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(4, loop2.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(*connection->connection_callbacks,
                    Abort(kTransactionId,
                          blink::kWebIDBDatabaseExceptionUnknownError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->open_callbacks,
                    Error(blink::kWebIDBDatabaseExceptionAbortError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->connection_callbacks, ForcedClose())
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(database_info));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        idb_mojo_factory_->AbortTransactionsAndCompactDatabase(base::BindOnce(
            &StatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();

  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          connection.reset();
                                          loop3.Quit();
                                        }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest,
       AbortTransactionsAfterCompletingTransaction) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  IDBDatabaseAssociatedPtrInfo database_info;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);
        {
          EXPECT_CALL(*connection->open_callbacks,
                      MockedUpgradeNeeded(
                          IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::mojom::IDBDataLoss::None, std::string(), _))
              .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                       testing::SaveArg<4>(&metadata),
                                       QuitLoop(&loop)));

          // Queue open request message.
          connection->Open(idb_mojo_factory_.get());
        }
      }));
  loop.Run();

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(4, loop2.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;
        EXPECT_CALL(*connection->connection_callbacks, Complete(kTransactionId))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->connection_callbacks, ForcedClose())
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(database_info));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        connection->version_change_transaction->Commit(0);
        idb_mojo_factory_->AbortTransactionsForDatabase(base::BindOnce(
            &StatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();

  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          connection.reset();
                                          loop3.Quit();
                                        }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest, AbortTransactionsWhileDoingTransaction) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  IDBDatabaseAssociatedPtrInfo database_info;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);

        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(4, loop2.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(*connection->connection_callbacks,
                    Abort(kTransactionId,
                          blink::kWebIDBDatabaseExceptionUnknownError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->open_callbacks,
                    Error(blink::kWebIDBDatabaseExceptionAbortError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->connection_callbacks, ForcedClose())
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(database_info));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        connection->version_change_transaction->CreateObjectStore(
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);
        idb_mojo_factory_->AbortTransactionsForDatabase(base::BindOnce(
            &StatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();

  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          connection.reset();
                                          loop3.Quit();
                                        }));
  loop3.Run();
}

TEST_F(IndexedDBDispatcherHostTest, AbortTransactionsWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  std::unique_ptr<TestDatabaseConnection> connection;
  IndexedDBDatabaseMetadata metadata;
  IDBDatabaseAssociatedPtrInfo database_info;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection.
        connection = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion, kTransactionId);

        EXPECT_CALL(*connection->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                     testing::SaveArg<4>(&metadata),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection->version, metadata.version);
  EXPECT_EQ(connection->db_name, metadata.name);

  blink::mojom::IDBStatus callback_result = blink::mojom::IDBStatus::IOError;

  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(4, loop2.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        EXPECT_CALL(*connection->connection_callbacks,
                    Abort(kTransactionId,
                          blink::kWebIDBDatabaseExceptionUnknownError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->open_callbacks,
                    Error(blink::kWebIDBDatabaseExceptionAbortError, _))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection->connection_callbacks, ForcedClose())
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));

        connection->database.Bind(std::move(database_info));
        ASSERT_TRUE(connection->database.is_bound());
        ASSERT_TRUE(connection->version_change_transaction.is_bound());
        idb_mojo_factory_->AbortTransactionsForDatabase(base::BindOnce(
            &StatusCallback, std::move(quit_closure2), &callback_result));
      }));
  loop2.Run();

  EXPECT_EQ(blink::mojom::IDBStatus::OK, callback_result);

  base::RunLoop loop3;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          connection.reset();
                                          loop3.Quit();
                                        }));
  loop3.Run();
}

// Flaky: crbug.com/772067
TEST_F(IndexedDBDispatcherHostTest, DISABLED_NotifyIndexedDBListChanged) {
  const int64_t kDBVersion1 = 1;
  const int64_t kDBVersion2 = 2;
  const int64_t kDBVersion3 = 3;
  const int64_t kTransactionId1 = 1;
  const int64_t kTransactionId2 = 2;
  const int64_t kTransactionId3 = 3;
  const int64_t kObjectStoreId = 10;
  const int64_t kIndexId = 100;
  const char kObjectStoreName[] = "os";
  const char kIndexName[] = "index";

  TestIndexedDBObserver observer;
  context_impl_->AddObserver(&observer);

  // Open connection 1.
  TestDatabaseConnection connection1(
      context_impl_->TaskRunner(), ToOrigin(kOrigin),
      base::UTF8ToUTF16(kDatabaseName), kDBVersion1, kTransactionId1);
  IndexedDBDatabaseMetadata metadata1;
  IDBDatabaseAssociatedPtrInfo database_info1;
  EXPECT_EQ(0, observer.notify_list_changed_count);
  {
    base::RunLoop loop;
    EXPECT_CALL(
        *connection1.open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::mojom::IDBDataLoss::None, std::string(), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info1),
                                 testing::SaveArg<4>(&metadata1),
                                 QuitLoop(&loop)));

    // Queue open request message.
    connection1.Open(idb_mojo_factory_.get());
    loop.Run();
  }
  EXPECT_TRUE(database_info1.is_valid());
  EXPECT_EQ(connection1.version, metadata1.version);
  EXPECT_EQ(connection1.db_name, metadata1.name);

  // Create object store and index.
  connection1.database.Bind(std::move(database_info1));
  ASSERT_TRUE(connection1.database.is_bound());
  ASSERT_TRUE(connection1.version_change_transaction.is_bound());
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(2, loop.QuitClosure());

    EXPECT_CALL(*connection1.connection_callbacks, Complete(kTransactionId1))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(
        *connection1.open_callbacks,
        MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(std::move(quit_closure)));

    ASSERT_TRUE(connection1.database.is_bound());
    connection1.version_change_transaction->CreateObjectStore(
        kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
        blink::IndexedDBKeyPath(), false);
    connection1.database->CreateIndex(kTransactionId1, kObjectStoreId, kIndexId,
                                      base::UTF8ToUTF16(kIndexName),
                                      blink::IndexedDBKeyPath(), false, false);
    connection1.version_change_transaction->Commit(0);
    loop.Run();
  }
  EXPECT_EQ(2, observer.notify_list_changed_count);
  connection1.database->Close();

  // Open connection 2.
  TestDatabaseConnection connection2(
      context_impl_->TaskRunner(), url::Origin::Create(GURL(kOrigin)),
      base::UTF8ToUTF16(kDatabaseName), kDBVersion2, kTransactionId2);
  IndexedDBDatabaseMetadata metadata2;
  IDBDatabaseAssociatedPtrInfo database_info2;
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    EXPECT_CALL(*connection2.open_callbacks,
                MockedUpgradeNeeded(
                    IsAssociatedInterfacePtrInfoValid(true), kDBVersion1,
                    blink::mojom::IDBDataLoss::None, std::string(), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info2),
                                 testing::SaveArg<4>(&metadata2),
                                 QuitLoop(&loop)));

    // Queue open request message.
    connection2.Open(idb_mojo_factory_.get());
    loop.Run();
  }
  EXPECT_TRUE(database_info2.is_valid());
  EXPECT_EQ(connection2.version, metadata2.version);
  EXPECT_EQ(connection2.db_name, metadata2.name);

  // Delete index.
  connection2.database.Bind(std::move(database_info2));
  ASSERT_TRUE(connection2.database.is_bound());
  ASSERT_TRUE(connection2.version_change_transaction.is_bound());
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(2, loop.QuitClosure());

    EXPECT_CALL(*connection2.connection_callbacks, Complete(kTransactionId2))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(
        *connection2.open_callbacks,
        MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(std::move(quit_closure)));

    ASSERT_TRUE(connection2.database.is_bound());
    connection2.database->DeleteIndex(kTransactionId2, kObjectStoreId,
                                      kIndexId);
    connection2.version_change_transaction->Commit(0);
    loop.Run();
  }
  EXPECT_EQ(3, observer.notify_list_changed_count);
  connection2.database->Close();

  // Open connection 3.
  TestDatabaseConnection connection3(
      context_impl_->TaskRunner(), ToOrigin(kOrigin),
      base::UTF8ToUTF16(kDatabaseName), kDBVersion3, kTransactionId3);
  IndexedDBDatabaseMetadata metadata3;
  IDBDatabaseAssociatedPtrInfo database_info3;
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    EXPECT_CALL(*connection3.open_callbacks,
                MockedUpgradeNeeded(
                    IsAssociatedInterfacePtrInfoValid(true), kDBVersion2,
                    blink::mojom::IDBDataLoss::None, std::string(), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info3),
                                 testing::SaveArg<4>(&metadata3),
                                 QuitLoop(&loop)));

    // Queue open request message.
    connection3.Open(idb_mojo_factory_.get());
    loop.Run();
  }
  EXPECT_TRUE(database_info3.is_valid());
  EXPECT_EQ(connection3.version, metadata3.version);
  EXPECT_EQ(connection3.db_name, metadata3.name);

  // Delete object store.
  connection3.database.Bind(std::move(database_info3));
  ASSERT_TRUE(connection3.database.is_bound());
  ASSERT_TRUE(connection3.version_change_transaction.is_bound());
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(2, loop.QuitClosure());

    EXPECT_CALL(*connection3.connection_callbacks, Complete(kTransactionId3))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(
        *connection3.open_callbacks,
        MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(std::move(quit_closure)));

    ASSERT_TRUE(connection3.database.is_bound());
    connection3.version_change_transaction->DeleteObjectStore(kObjectStoreId);
    connection3.version_change_transaction->Commit(0);
    loop.Run();
  }
  EXPECT_EQ(4, observer.notify_list_changed_count);

  context_impl_->RemoveObserver(&observer);
}

// The test is flaky. See https://crbug.com/879213
TEST_F(IndexedDBDispatcherHostTest, DISABLED_NotifyIndexedDBContentChanged) {
  const int64_t kDBVersion1 = 1;
  const int64_t kDBVersion2 = 2;
  const int64_t kTransactionId1 = 1;
  const int64_t kTransactionId2 = 2;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";

  TestIndexedDBObserver observer;
  context_impl_->AddObserver(&observer);
  EXPECT_EQ(0, observer.notify_list_changed_count);
  EXPECT_EQ(0, observer.notify_content_changed_count);

  std::unique_ptr<TestDatabaseConnection> connection1;
  IndexedDBDatabaseMetadata metadata1;
  IDBDatabaseAssociatedPtrInfo database_info1;

  base::RunLoop loop;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Open connection 1.
        connection1 = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), url::Origin::Create(GURL(kOrigin)),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion1, kTransactionId1);

        EXPECT_CALL(*connection1->open_callbacks,
                    MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                                        IndexedDBDatabaseMetadata::NO_VERSION,
                                        blink::mojom::IDBDataLoss::None,
                                        std::string(), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info1),
                                     testing::SaveArg<4>(&metadata1),
                                     QuitLoop(&loop)));

        // Queue open request message.
        connection1->Open(idb_mojo_factory_.get());
      }));
  loop.Run();

  EXPECT_TRUE(database_info1.is_valid());
  EXPECT_EQ(connection1->version, metadata1.version);
  EXPECT_EQ(connection1->db_name, metadata1.name);

  std::unique_ptr<StrictMock<MockMojoIndexedDBCallbacks>> put_callbacks;

  // Add object store entry.
  base::RunLoop loop2;
  base::RepeatingClosure quit_closure2 =
      base::BarrierClosure(3, loop2.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        put_callbacks =
            std::make_unique<StrictMock<MockMojoIndexedDBCallbacks>>();

        EXPECT_CALL(*put_callbacks, SuccessKey(_))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(*connection1->connection_callbacks,
                    Complete(kTransactionId1))
            .Times(1)
            .WillOnce(RunClosure(quit_closure2));
        EXPECT_CALL(
            *connection1->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(std::move(quit_closure2)));

        connection1->database.Bind(std::move(database_info1));
        ASSERT_TRUE(connection1->database.is_bound());
        ASSERT_TRUE(connection1->version_change_transaction.is_bound());
        connection1->version_change_transaction->CreateObjectStore(
            kObjectStoreId, base::UTF8ToUTF16(kObjectStoreName),
            blink::IndexedDBKeyPath(), false);

        std::string value = "value";
        const char* value_data = value.data();
        std::vector<uint8_t> value_vector(value_data,
                                          value_data + value.length());

        auto new_value = blink::mojom::IDBValue::New();
        new_value->bits = std::move(value_vector);
        new_value->blob_or_file_info =
            std::vector<blink::mojom::IDBBlobInfoPtr>();

        connection1->version_change_transaction->Put(
            kObjectStoreId, std::move(new_value),
            IndexedDBKey(base::UTF8ToUTF16("key")),
            blink::mojom::IDBPutMode::AddOnly,
            std::vector<IndexedDBIndexKeys>(),
            put_callbacks->CreateInterfacePtrAndBind());
        connection1->version_change_transaction->Commit(0);
      }));
  loop2.Run();

  EXPECT_EQ(2, observer.notify_list_changed_count);
  EXPECT_EQ(1, observer.notify_content_changed_count);

  base::RunLoop loop3;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          connection1->database->Close();
                                          put_callbacks.reset();
                                          connection1.reset();
                                          loop3.Quit();
                                        }));
  loop3.Run();

  std::unique_ptr<TestDatabaseConnection> connection2;
  IndexedDBDatabaseMetadata metadata2;
  IDBDatabaseAssociatedPtrInfo database_info2;

  // Open connection 2.
  base::RunLoop loop4;
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        connection2 = std::make_unique<TestDatabaseConnection>(
            context_impl_->TaskRunner(), ToOrigin(kOrigin),
            base::UTF8ToUTF16(kDatabaseName), kDBVersion2, kTransactionId2);

        EXPECT_CALL(*connection2->open_callbacks,
                    MockedUpgradeNeeded(
                        IsAssociatedInterfacePtrInfoValid(true), kDBVersion1,
                        blink::mojom::IDBDataLoss::None, std::string(), _))
            .WillOnce(testing::DoAll(MoveArg<0>(&database_info2),
                                     testing::SaveArg<4>(&metadata2),
                                     QuitLoop(&loop4)));

        // Queue open request message.
        connection2->Open(idb_mojo_factory_.get());
      }));
  loop4.Run();

  EXPECT_TRUE(database_info2.is_valid());
  EXPECT_EQ(connection2->version, metadata2.version);
  EXPECT_EQ(connection2->db_name, metadata2.name);

  std::unique_ptr<StrictMock<MockMojoIndexedDBCallbacks>> clear_callbacks;

  // Clear object store.
  base::RunLoop loop5;
  base::RepeatingClosure quit_closure5 =
      base::BarrierClosure(3, loop5.QuitClosure());
  context_impl_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ::testing::InSequence dummy;

        clear_callbacks =
            std::make_unique<StrictMock<MockMojoIndexedDBCallbacks>>();

        EXPECT_CALL(*clear_callbacks, Success())
            .Times(1)
            .WillOnce(RunClosure(quit_closure5));
        EXPECT_CALL(*connection2->connection_callbacks,
                    Complete(kTransactionId2))
            .Times(1)
            .WillOnce(RunClosure(quit_closure5));
        EXPECT_CALL(
            *connection2->open_callbacks,
            MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
            .Times(1)
            .WillOnce(RunClosure(std::move(quit_closure5)));

        connection2->database.Bind(std::move(database_info2));
        ASSERT_TRUE(connection2->database.is_bound());
        ASSERT_TRUE(connection2->version_change_transaction.is_bound());
        connection2->database->Clear(
            kTransactionId2, kObjectStoreId,
            clear_callbacks->CreateInterfacePtrAndBind());
        connection2->version_change_transaction->Commit(0);
      }));
  loop5.Run();

  EXPECT_EQ(3, observer.notify_list_changed_count);
  EXPECT_EQ(2, observer.notify_content_changed_count);

  base::RunLoop loop6;
  context_impl_->TaskRunner()->PostTask(FROM_HERE,
                                        base::BindLambdaForTesting([&]() {
                                          clear_callbacks.reset();
                                          connection2.reset();
                                          loop6.Quit();
                                        }));
  loop6.Run();

  context_impl_->RemoveObserver(&observer);
}

}  // namespace content
