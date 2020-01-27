// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/public/browser/indexed_db_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/blob/mojom/blob_storage_context.mojom.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/origin.h"

namespace base {
class Clock;
class ListValue;
class FilePath;
class SequencedTaskRunner;
}

namespace url {
class Origin;
}

namespace content {
class IndexedDBConnection;
class IndexedDBFactoryImpl;

class CONTENT_EXPORT IndexedDBContextImpl
    : public IndexedDBContext,
      public storage::mojom::IndexedDBControl {
 public:
  class Observer {
   public:
    virtual void OnIndexedDBListChanged(const url::Origin& origin) = 0;
    virtual void OnIndexedDBContentChanged(
        const url::Origin& origin,
        const base::string16& database_name,
        const base::string16& object_store_name) = 0;

   protected:
    virtual ~Observer() {}
  };

  // The indexed db directory.
  static const base::FilePath::CharType kIndexedDBDirectory[];

  // If |data_path| is empty, nothing will be saved to disk.
  // |task_runner| is optional, and only set during testing.
  // This is called on the UI thread.
  IndexedDBContextImpl(
      const base::FilePath& data_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::Clock* clock,
      mojo::PendingRemote<storage::mojom::BlobStorageContext>
          blob_storage_context,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      scoped_refptr<base::SequencedTaskRunner> custom_task_runner);

  void Bind(mojo::PendingReceiver<storage::mojom::IndexedDBControl> control);

  // mojom::IndexedDBControl implementation:
  void GetUsage(GetUsageCallback usage_callback) override;
  void DeleteForOrigin(const url::Origin& origin,
                       DeleteForOriginCallback callback) override;
  void ForceClose(const url::Origin& origin,
                  storage::mojom::ForceCloseReason reason,
                  base::OnceClosure callback) override;
  void GetConnectionCount(const url::Origin& origin,
                          GetConnectionCountCallback callback) override;
  void DownloadOriginData(const url::Origin& origin,
                          DownloadOriginDataCallback callback) override;
  void GetAllOriginsDetails(GetAllOriginsDetailsCallback callback) override;

  // TODO(enne): fix internal indexeddb callers to use ForceClose async instead.
  void ForceCloseSync(const url::Origin& origin,
                      storage::mojom::ForceCloseReason reason);

  IndexedDBFactoryImpl* GetIDBFactory();

  base::SequencedTaskRunner* IOTaskRunner();

  // Called by StoragePartitionImpl to clear session-only data.
  void Shutdown();

  int64_t GetOriginDiskUsage(const url::Origin& origin);

  // IndexedDBContext implementation:
  base::SequencedTaskRunner* IDBTaskRunner() override;
  void CopyOriginData(const url::Origin& origin,
                      IndexedDBContext* dest_context) override;
  base::FilePath GetFilePathForTesting(const url::Origin& origin) override;
  void ResetCachesForTesting() override;
  void SetForceKeepSessionState() override;

  // Methods called by IndexedDBFactoryImpl or IndexedDBDispatcherHost for
  // quota support.
  void FactoryOpened(const url::Origin& origin);
  void ConnectionOpened(const url::Origin& origin, IndexedDBConnection* db);
  void ConnectionClosed(const url::Origin& origin, IndexedDBConnection* db);
  void TransactionComplete(const url::Origin& origin);
  void DatabaseDeleted(const url::Origin& origin);

  // Called when blob files have been cleaned (an aggregated delayed task).
  void BlobFilesCleaned(const url::Origin& origin);

  // Will be null in unit tests.
  storage::QuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

  // Returns a list of all origins with backing stores.
  std::vector<url::Origin> GetAllOrigins();
  bool HasOrigin(const url::Origin& origin);

  // Used by IndexedDBInternalsUI to populate internals page.
  base::ListValue* GetAllOriginsDetails();

  bool ForceSchemaDowngrade(const url::Origin& origin);
  V2SchemaCorruptionStatus HasV2SchemaCorruption(const url::Origin& origin);
  // GetStoragePaths returns all paths owned by this database, in arbitrary
  // order.
  std::vector<base::FilePath> GetStoragePaths(const url::Origin& origin) const;

  base::FilePath data_path() const { return data_path_; }
  bool IsInMemoryContext() const { return data_path_.empty(); }
  size_t GetConnectionCountSync(const url::Origin& origin);
  int GetOriginBlobFileCount(const url::Origin& origin);

  // For unit tests allow to override the |data_path_|.
  void set_data_path_for_testing(const base::FilePath& data_path) {
    data_path_ = data_path;
  }

  bool is_incognito() const { return data_path_.empty(); }

  // Only callable on the IDB task runner.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyIndexedDBListChanged(const url::Origin& origin);
  void NotifyIndexedDBContentChanged(const url::Origin& origin,
                                     const base::string16& database_name,
                                     const base::string16& object_store_name);

 protected:
  ~IndexedDBContextImpl() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ClearLocalState);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ClearSessionOnlyDatabases);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, SetForceKeepSessionState);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ForceCloseOpenDatabasesOnDelete);
  friend class IndexedDBQuotaClientTest;

  class IndexedDBGetUsageAndQuotaCallback;

  static void ClearSessionOnlyOrigins(
      const base::FilePath& indexeddb_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  base::FilePath GetBlobStorePath(const url::Origin& origin) const;
  base::FilePath GetLevelDBPath(const url::Origin& origin) const;

  int64_t ReadUsageFromDisk(const url::Origin& origin) const;
  void EnsureDiskUsageCacheInitialized(const url::Origin& origin);
  void QueryDiskAndUpdateQuotaUsage(const url::Origin& origin);
  base::Time GetOriginLastModified(const url::Origin& origin);

  // Returns |origin_set_| (this context's in-memory cache of origins with
  // backing stores); the cache will be primed as needed by checking disk.
  std::set<url::Origin>* GetOriginSet();

  // Bound and accessed on the |idb_task_runner_|.
  mojo::Remote<storage::mojom::BlobStorageContext> blob_storage_context_;
  std::unique_ptr<IndexedDBFactoryImpl> indexeddb_factory_;

  // If |data_path_| is empty then this is an incognito session and the backing
  // store will be held in-memory rather than on-disk.
  base::FilePath data_path_;

  // If true, nothing (not even session-only data) should be deleted on exit.
  bool force_keep_session_state_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<base::SequencedTaskRunner> idb_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<std::set<url::Origin>> origin_set_;
  std::map<url::Origin, int64_t> origin_size_map_;
  base::ObserverList<Observer>::Unchecked observers_;
  base::Clock* clock_;

  mojo::ReceiverSet<storage::mojom::IndexedDBControl> receivers_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
