// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/test/test_arc_session_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"

namespace arc {
namespace {

bool CreateFilesAndDirectories(const base::FilePath& temp_dir,
                               base::FilePath* source_dir,
                               base::FilePath* dest_dir) {
  if (!base::CreateTemporaryDirInDir(temp_dir, "source", source_dir))
    return false;

  // Create empty prop files so ArcSessionManager's property expansion code
  // works like production.
  for (const char* filename : {"build.prop", "default.prop"}) {
    if (base::WriteFile(source_dir->Append(filename), "", 1) != 1)
      return false;
  }

  return base::CreateTemporaryDirInDir(temp_dir, "dest", dest_dir);
}

}  // namespace

std::unique_ptr<ArcSessionManager> CreateTestArcSessionManager(
    std::unique_ptr<ArcSessionRunner> arc_session_runner) {
  auto manager =
      std::make_unique<ArcSessionManager>(std::move(arc_session_runner));
  // Our unit tests the ArcSessionManager::ExpandPropertyFiles() function won't
  // be automatically called. Because of that, we can call
  // OnExpandPropertyFilesForTesting() instead with |true| for easier unit
  // testing (without calling base::RunLoop().RunUntilIdle() here and there.)
  manager->OnExpandPropertyFilesForTesting(true);
  return manager;
}

bool ExpandPropertyFilesForTesting(ArcSessionManager* arc_session_manager,
                                   const base::FilePath& temp_dir) {
  // For browser_tests, do the actual prop file expansion to make it more
  // similar to production. Calling ExpandPropertyFiles() here is fine as long
  // as the caller doesn't explicitly call ArcServiceLauncher::Initialize()
  // after recreating ASM with ArcServiceLauncher::ResetForTesting().
  base::FilePath source_dir, dest_dir;
  if (!CreateFilesAndDirectories(temp_dir, &source_dir, &dest_dir))
    return false;
  arc_session_manager->set_property_files_source_dir_for_testing(source_dir);
  arc_session_manager->set_property_files_dest_dir_for_testing(dest_dir);
  arc_session_manager->ExpandPropertyFiles();
  return true;
}

}  // namespace arc
