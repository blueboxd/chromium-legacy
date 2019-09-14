// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_DATABASE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_DATABASE_IMPL_H_

#include <unordered_map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/history/print_job_database.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace base {
class FilePath;
}  // namespace base

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace chromeos {

class PrintJobDatabaseImpl : public PrintJobDatabase {
 public:
  PrintJobDatabaseImpl(leveldb_proto::ProtoDatabaseProvider* database_provider,
                       base::FilePath profile_path);
  ~PrintJobDatabaseImpl() override;

  // PrintJobDatabase:
  void Initialize(InitializeCallback callback) override;
  bool IsInitialized() override;
  void SavePrintJob(const printing::proto::PrintJobInfo& print_job_info,
                    SavePrintJobCallback callback) override;
  void DeletePrintJob(const std::string& id,
                      DeletePrintJobCallback callback) override;
  void GetPrintJobs(GetPrintJobsCallback callback) override;

 private:
  friend class PrintJobDatabaseImplTest;

  void OnInitialized(InitializeCallback callback,
                     leveldb_proto::Enums::InitStatus status);

  void OnKeysAndEntriesLoaded(
      InitializeCallback callback,
      bool success,
      std::unique_ptr<std::map<std::string, printing::proto::PrintJobInfo>>
          entries);

  void FinishInitialization(InitializeCallback callback, bool success);

  void OnPrintJobSaved(const printing::proto::PrintJobInfo& print_job_info,
                       SavePrintJobCallback callback,
                       bool success);

  void OnPrintJobDeleted(const std::string& id,
                         DeletePrintJobCallback callback,
                         bool success);

  void GetPrintJobsFromProtoDatabase(GetPrintJobsCallback callback);

  void OnPrintJobsRetrieved(
      GetPrintJobsCallback callback,
      bool success,
      std::unique_ptr<std::vector<printing::proto::PrintJobInfo>> entries);

  // The persistent ProtoDatabase for storing print job information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<printing::proto::PrintJobInfo>>
      database_;

  // Cached PrintJobInfo entries.
  std::unordered_map<std::string, printing::proto::PrintJobInfo> cache_;

  // Whether or not the ProtoDatabase database has been initialized and entries
  // have been loaded.
  bool is_initialized_ = false;

  // Number of initialize attempts.
  int initialize_attempts_ = 0;

  base::WeakPtrFactory<PrintJobDatabaseImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrintJobDatabaseImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_DATABASE_IMPL_H_
