// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_sql.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "sql/database.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ConversionStorageSqlTest : public testing::Test {
 public:
  ConversionStorageSqlTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void OpenDatabase() {
    storage_.reset();
    storage_ = std::make_unique<ConversionStorageSql>(temp_directory_.GetPath(),
                                                      &delegate_, &clock_);
    EXPECT_TRUE(storage_->Initialize());
  }

  void CloseDatabase() { storage_.reset(); }

  void AddReportToStorage() {
    storage_->StoreImpression(ImpressionBuilder(clock()->Now()).Build());
    storage_->MaybeCreateAndStoreConversionReports(DefaultConversion());
  }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions"));
  }

  base::SimpleTestClock* clock() { return &clock_; }

  ConversionStorage* storage() { return storage_.get(); }

 private:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<ConversionStorage> storage_;
  base::SimpleTestClock clock_;
  EmptyStorageDelegate delegate_;
};

TEST_F(ConversionStorageSqlTest,
       DatabaseInitialized_TablesAndIndexesInitialized) {
  OpenDatabase();
  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  // [impressions] and [conversions].
  EXPECT_EQ(2u, sql::test::CountSQLTables(&raw_db));

  // [conversion_origin_idx], [impression_expiry_idx],
  // [conversion_report_time_idx], [conversion_impression_id_idx].
  EXPECT_EQ(4u, sql::test::CountSQLIndices(&raw_db));
}

TEST_F(ConversionStorageSqlTest, DatabaseReopened_DataPersisted) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());
  CloseDatabase();
  OpenDatabase();
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());
}

TEST_F(ConversionStorageSqlTest, CorruptDatabase_RecoveredOnOpen) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());
  CloseDatabase();

  // Corrupt the database.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(db_path()));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CORRUPT);

  // Open that database and ensure that it does not fail.
  EXPECT_NO_FATAL_FAILURE(OpenDatabase());

  // Data should be recovered.
  EXPECT_EQ(1u, storage()->GetConversionsToReport(clock()->Now()).size());

  EXPECT_TRUE(expecter.SawExpectedErrors());
}

}  // namespace content
