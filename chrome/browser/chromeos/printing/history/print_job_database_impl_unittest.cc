// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_database_impl.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using printing::proto::PrintJobInfo;

namespace {

constexpr char kId1[] = "id1";
constexpr char kId2[] = "id2";
constexpr char kTitle1[] = "title1";
constexpr char kTitle2[] = "title2";

PrintJobInfo ConstructPrintJobInfo(const std::string& id,
                                   const std::string& title) {
  PrintJobInfo print_job_info;
  print_job_info.set_id(id);
  print_job_info.set_title(title);
  return print_job_info;
}

}  // namespace

class PrintJobDatabaseImplTest : public ::testing::Test {
 public:
  PrintJobDatabaseImplTest() {}

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    database_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    print_job_database_ = std::make_unique<PrintJobDatabaseImpl>(
        database_provider_.get(), temp_dir_.GetPath());
  }

  void TearDown() override {
    database_provider_.reset();
    print_job_database_.reset();
  }

  void OnPrintJobSaved(base::RepeatingClosure run_loop_closure, bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

 protected:
  void Initialize() {
    base::RunLoop run_loop;
    print_job_database_->Initialize(
        base::BindOnce(&PrintJobDatabaseImplTest::OnInitialized,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnInitialized(base::RepeatingClosure run_loop_closure, bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

  void SavePrintJob(const PrintJobInfo& print_job_info) {
    base::RunLoop run_loop;
    print_job_database_->SavePrintJob(
        print_job_info,
        base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobSaved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void DeletePrintJob(const std::string& id) {
    base::RunLoop run_loop;
    print_job_database_->DeletePrintJob(
        id, base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobDeleted,
                           base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnPrintJobDeleted(base::RepeatingClosure run_loop_closure,
                         bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

  std::vector<PrintJobInfo> GetPrintJobs() {
    base::RunLoop run_loop;
    print_job_database_->GetPrintJobs(
        base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobsRetrieved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return entries_;
  }

  std::vector<PrintJobInfo> GetPrintJobsFromProtoDatabase() {
    base::RunLoop run_loop;
    print_job_database_->GetPrintJobsFromProtoDatabase(
        base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobsRetrieved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return entries_;
  }

  void OnPrintJobsRetrieved(
      base::RepeatingClosure run_loop_closure,
      bool success,
      std::unique_ptr<std::vector<PrintJobInfo>> entries) {
    EXPECT_TRUE(success);
    entries_ = *entries;
    run_loop_closure.Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrintJobDatabaseImpl> print_job_database_;

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> database_provider_;
  std::vector<PrintJobInfo> entries_;
};

TEST_F(PrintJobDatabaseImplTest, Initialize) {
  Initialize();
  EXPECT_TRUE(print_job_database_->IsInitialized());
}

TEST_F(PrintJobDatabaseImplTest, SavePrintJob) {
  Initialize();
  PrintJobInfo print_job_info = ConstructPrintJobInfo(kId1, kTitle1);
  SavePrintJob(print_job_info);
  std::vector<PrintJobInfo> entries = GetPrintJobs();
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(kId1, entries[0].id());
  EXPECT_EQ(kTitle1, entries[0].title());
}

TEST_F(PrintJobDatabaseImplTest, DeletePrintJob) {
  Initialize();
  PrintJobInfo print_job_info1 = ConstructPrintJobInfo(kId1, kTitle1);
  SavePrintJob(print_job_info1);
  PrintJobInfo print_job_info2 = ConstructPrintJobInfo(kId2, kTitle2);
  SavePrintJob(print_job_info2);
  DeletePrintJob(kId1);
  std::vector<PrintJobInfo> entries = GetPrintJobs();
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(kId2, entries[0].id());
  EXPECT_EQ(kTitle2, entries[0].title());
}

TEST_F(PrintJobDatabaseImplTest, GetPrintJobsFromDatabase) {
  Initialize();
  PrintJobInfo print_job_info = ConstructPrintJobInfo(kId1, kTitle1);
  SavePrintJob(print_job_info);
  std::vector<PrintJobInfo> entries = GetPrintJobsFromProtoDatabase();
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(kId1, entries[0].id());
  EXPECT_EQ(kTitle1, entries[0].title());
}

TEST_F(PrintJobDatabaseImplTest, TwoSimultaneousSavePrintJobRequests) {
  Initialize();
  PrintJobInfo print_job_info = ConstructPrintJobInfo(kId1, kTitle1);
  PrintJobInfo print_job_info2 = ConstructPrintJobInfo(kId2, kTitle2);

  base::RunLoop run_loop;
  base::RunLoop run_loop2;
  print_job_database_->SavePrintJob(
      print_job_info,
      base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobSaved,
                     base::Unretained(this), run_loop.QuitClosure()));
  print_job_database_->SavePrintJob(
      print_job_info2,
      base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobSaved,
                     base::Unretained(this), run_loop2.QuitClosure()));
  run_loop.Run();
  run_loop2.Run();

  std::vector<PrintJobInfo> entries = GetPrintJobsFromProtoDatabase();
  EXPECT_EQ(2u, entries.size());
  std::vector<std::string> ids = {entries[0].id(), entries[1].id()};
  EXPECT_TRUE(std::find(ids.begin(), ids.end(), kId1) != ids.end());
  EXPECT_TRUE(std::find(ids.begin(), ids.end(), kId2) != ids.end());
}

}  // namespace chromeos
