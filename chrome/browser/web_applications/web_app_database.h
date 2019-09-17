// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/web_applications/abstract_web_app_sync_bridge.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {
class ModelError;
}  // namespace syncer

namespace web_app {

class AbstractWebAppDatabaseFactory;
class WebApp;
class WebAppProto;

// Exclusively used from the UI thread.
class WebAppDatabase {
 public:
  explicit WebAppDatabase(AbstractWebAppDatabaseFactory* database_factory);
  ~WebAppDatabase();

  using RegistryOpenedCallback =
      AbstractWebAppSyncBridge::RegistryOpenedCallback;
  using CompletionCallback = AbstractWebAppSyncBridge::CompletionCallback;
  using AppsToWrite = AbstractWebAppSyncBridge::AppsToWrite;

  void OpenDatabase(RegistryOpenedCallback callback);
  void WriteWebApps(AppsToWrite apps, CompletionCallback callback);
  void DeleteWebApps(std::vector<AppId> app_ids, CompletionCallback callback);

  // Exposed for testing.
  static std::unique_ptr<WebAppProto> CreateWebAppProto(const WebApp& web_app);
  // Exposed for testing.
  static std::unique_ptr<WebApp> CreateWebApp(const WebAppProto& proto);
  // Exposed for testing.
  void ReadRegistry(RegistryOpenedCallback callback);

 private:
  void CreateStore(syncer::OnceModelTypeStoreFactory store_factory,
                   base::OnceClosure closure);
  void OnStoreCreated(base::OnceClosure closure,
                      const base::Optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);

  void OnAllDataRead(
      RegistryOpenedCallback callback,
      const base::Optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records);

  void OnDataWritten(CompletionCallback callback,
                     const base::Optional<syncer::ModelError>& error);

  static std::unique_ptr<WebApp> ParseWebApp(const AppId& app_id,
                                             const std::string& value);

  void BeginTransaction();
  void CommitTransaction(CompletionCallback callback);

  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch_;
  AbstractWebAppDatabaseFactory* database_factory_;

  // Database is opened if store is created and all data read.
  bool opened_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebAppDatabase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppDatabase);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_H_
