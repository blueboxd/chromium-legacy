// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/leveldb/leveldb_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "components/services/leveldb/leveldb_database_impl.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace leveldb {

namespace {

void CreateReceiver(
    std::unique_ptr<LevelDBDatabaseImpl> db,
    mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> receiver) {
  // The database should be able to close the binding if it gets into an
  // error condition that can't be recovered.
  LevelDBDatabaseImpl* impl = db.get();
  impl->SetCloseBindingClosure(base::BindOnce(
      &mojo::StrongAssociatedBinding<mojom::LevelDBDatabase>::Close,
      mojo::MakeSelfOwnedAssociatedReceiver(std::move(db),
                                            std::move(receiver))));
}

class LevelDBDatabaseEnv : public leveldb_env::ChromiumEnv {
 public:
  LevelDBDatabaseEnv() : ChromiumEnv("ChromiumEnv.LevelDBService") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LevelDBDatabaseEnv);
};

std::string MakeFullPersistentDBName(const base::FilePath& directory,
                                     const std::string& db_name) {
  // ChromiumEnv treats DB name strings as UTF-8 file paths.
  return directory.Append(base::FilePath::FromUTF8Unsafe(db_name))
      .AsUTF8Unsafe();
}

LevelDBDatabaseEnv* GetLevelDBDatabaseEnv() {
  static base::NoDestructor<LevelDBDatabaseEnv> env;
  return env.get();
}

}  // namespace

LevelDBServiceImpl::LevelDBServiceImpl() = default;

LevelDBServiceImpl::~LevelDBServiceImpl() = default;

void LevelDBServiceImpl::Open(
    const base::FilePath& directory,
    const std::string& dbname,
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> database,
    OpenCallback callback) {
  leveldb_env::Options options;
  // the default here to 80 instead of leveldb's default 1000 because we don't
  // want to consume all file descriptors. See
  // https://code.google.com/p/chromium/issues/detail?id=227313#c11 for
  // details.)
  options.max_open_files = 80;

  OpenWithOptions(options, directory, dbname, memory_dump_id,
                  std::move(database), std::move(callback));
}

void LevelDBServiceImpl::OpenWithOptions(
    const leveldb_env::Options& options,
    const base::FilePath& directory,
    const std::string& dbname,
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> database,
    OpenCallback callback) {
  leveldb_env::Options open_options = options;
  open_options.env = GetLevelDBDatabaseEnv();

  std::string fullname = MakeFullPersistentDBName(directory, dbname);
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status s = leveldb_env::OpenDB(open_options, fullname, &db);

  if (s.ok()) {
    CreateReceiver(std::make_unique<LevelDBDatabaseImpl>(
                       nullptr, std::move(db), nullptr, open_options, fullname,
                       memory_dump_id),
                   std::move(database));
  }

  std::move(callback).Run(LeveldbStatusToError(s));
}

void LevelDBServiceImpl::OpenInMemory(
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    const std::string& tracking_name,
    mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> database,
    OpenCallback callback) {
  leveldb_env::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum.

  auto env = leveldb_chrome::NewMemEnv(tracking_name);
  options.env = env.get();

  std::unique_ptr<leveldb::DB> db;
  leveldb::Status s = leveldb_env::OpenDB(options, "", &db);

  if (s.ok()) {
    CreateReceiver(std::make_unique<LevelDBDatabaseImpl>(
                       std::move(env), std::move(db), nullptr, options,
                       tracking_name, memory_dump_id),
                   std::move(database));
  }

  std::move(callback).Run(LeveldbStatusToError(s));
}

void LevelDBServiceImpl::Destroy(const base::FilePath& directory,
                                 const std::string& dbname,
                                 DestroyCallback callback) {
  leveldb_env::Options options;
  options.env = GetLevelDBDatabaseEnv();
  std::move(callback).Run(LeveldbStatusToError(leveldb::DestroyDB(
      MakeFullPersistentDBName(directory, dbname), options)));
}

}  // namespace leveldb
