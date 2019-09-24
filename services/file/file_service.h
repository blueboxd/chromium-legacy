// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_FILE_FILE_SERVICE_H_
#define SERVICES_FILE_FILE_SERVICE_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/leveldb/public/mojom/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/file/public/mojom/file_system.mojom.h"

namespace file {

class FileService {
 public:
  explicit FileService(const base::FilePath& directory);
  ~FileService();

  void BindFileSystem(mojo::PendingReceiver<mojom::FileSystem> receiver);
  void BindLevelDBService(
      mojo::PendingReceiver<leveldb::mojom::LevelDBService> receiver);

  using LevelDBBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<leveldb::mojom::LevelDBService>)>;
  void OverrideLevelDBBinderForTesting(LevelDBBinder binder) {
    leveldb_binder_override_ = std::move(binder);
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> file_service_runner_;
  scoped_refptr<base::SequencedTaskRunner> leveldb_service_runner_;

  // We create these two objects so we can delete them on the correct task
  // runners.
  class FileSystemObjects;
  std::unique_ptr<FileSystemObjects> file_system_objects_;

  LevelDBBinder leveldb_binder_override_;

  DISALLOW_COPY_AND_ASSIGN(FileService);
};

}  // namespace file

#endif  // SERVICES_FILE_FILE_SERVICE_H_
