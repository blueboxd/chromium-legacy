// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_LEVELDB_LEVELDB_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_LEVELDB_LEVELDB_SERVICE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/services/leveldb/public/mojom/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace leveldb {

// Creates LevelDBDatabases based scoped to a |directory|/|dbname|.
class LevelDBServiceImpl : public mojom::LevelDBService {
 public:
  LevelDBServiceImpl();
  ~LevelDBServiceImpl() override;

  // Overridden from LevelDBService:
  void Open(
      const base::FilePath& directory,
      const std::string& dbname,
      const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> database,
      OpenCallback callback) override;
  void OpenWithOptions(
      const leveldb_env::Options& open_options,
      const base::FilePath& directory,
      const std::string& dbname,
      const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> database,
      OpenCallback callback) override;
  void OpenInMemory(
      const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      const std::string& tracking_name,
      mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> database,
      OpenInMemoryCallback callback) override;
  void Destroy(const base::FilePath& directory,
               const std::string& dbname,
               DestroyCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LevelDBServiceImpl);
};

}  // namespace leveldb

#endif  // COMPONENTS_SERVICES_LEVELDB_LEVELDB_SERVICE_IMPL_H_
