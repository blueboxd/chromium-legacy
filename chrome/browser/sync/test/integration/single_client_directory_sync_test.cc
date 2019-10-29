// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/syncable/directory.h"
#include "sql/test/test_helpers.h"
#include "url/gurl.h"

using base::FileEnumerator;
using base::FilePath;

namespace {

// USS ModelTypeStore uses the same folder as the Directory. However, all of its
// content is in a sub-folder. By not asking for recursive files, this function
// will avoid seeing any of those, and return iff Directory database files still
// exist.
bool FolderContainsFiles(const FilePath& folder) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (base::DirectoryExists(folder)) {
    return !FileEnumerator(folder, false, FileEnumerator::FILES).Next().empty();
  } else {
    return false;
  }
}

class SingleClientDirectorySyncTest : public SyncTest {
 public:
  SingleClientDirectorySyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientDirectorySyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientDirectorySyncTest);
};

// A status change checker that waits for an unrecoverable sync error to occur.
class SyncUnrecoverableErrorChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncUnrecoverableErrorChecker(syncer::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for a Sync Unrecoverable Error";
    return service()->HasUnrecoverableError();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientDirectorySyncTest,
                       StopThenDisableDeletesDirectory) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  syncer::ProfileSyncService* sync_service = GetSyncService(0);
  FilePath directory_path = sync_service->GetSyncClientForTest()
                                ->GetSyncDataPath();
  ASSERT_TRUE(FolderContainsFiles(directory_path));
  sync_service->StopAndClear();

  // Wait for StartupController::StartUp()'s tasks to finish.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  // Wait for the directory deletion to finish.
  sync_service->FlushBackendTaskRunnerForTest();
  EXPECT_FALSE(FolderContainsFiles(directory_path));
}

// Verify that when the sync directory's backing store becomes corrupted, we
// trigger an unrecoverable error and delete the database.
IN_PROC_BROWSER_TEST_F(SingleClientDirectorySyncTest,
                       DeleteDirectoryWhenCorrupted) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Sync and wait for syncing to complete.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(bookmarks_helper::ModelMatchesVerifier(0));

  // Flush the directory to the backing store and wait until the flush
  // completes.
  syncer::ProfileSyncService* sync_service = GetSyncService(0);
  sync_service->FlushDirectory();
  sync_service->FlushBackendTaskRunnerForTest();

  // Now corrupt the database.
  FilePath directory_path = sync_service->GetSyncClientForTest()
                                ->GetSyncDataPath();
  const FilePath sync_db(directory_path.Append(
      syncer::syncable::Directory::kSyncDatabaseFilename));
  ASSERT_TRUE(sql::test::CorruptSizeInHeaderWithLock(sync_db));

  // Write some bookmarks and flush the directory to force sync to
  // notice the corruption.
  const GURL url("https://www.example.com");
  const bookmarks::BookmarkNode* top = bookmarks_helper::AddFolder(
      0, bookmarks_helper::GetOtherNode(0), 0, "top");
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(
        bookmarks_helper::AddURL(0, top, 0, base::NumberToString(i), url));
  }
  sync_service->FlushDirectory();

  // Wait for an unrecoverable error to occur.
  ASSERT_TRUE(SyncUnrecoverableErrorChecker(sync_service).Wait());
  ASSERT_TRUE(sync_service->HasUnrecoverableError());

  // An unrecoverable error causes ProfileSyncService to post a shutdown task to
  // its task runner. Make sure that task gets run before we wait for the sync
  // thread.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();

  // Wait until the sync loop has processed any existing tasks and see that the
  // directory no longer exists.
  sync_service->FlushBackendTaskRunnerForTest();
  ASSERT_FALSE(FolderContainsFiles(directory_path));
}

}  // namespace
