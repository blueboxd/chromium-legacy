// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_

#include <vector>

#include "base/files/file_path.h"

class Profile;

namespace file_manager {
namespace test {

// A dummy folder in a temporary path that is automatically mounted as a
// Profile's Downloads folder.
class FolderInMyFiles {
 public:
  explicit FolderInMyFiles(Profile* profile);
  ~FolderInMyFiles();

  // Copies additional files into |folder_|, adding to |files_|.
  void Add(const std::vector<base::FilePath>& files);

  const std::vector<base::FilePath> files() { return files_; }

 private:
  FolderInMyFiles(const FolderInMyFiles&) = delete;
  FolderInMyFiles& operator=(const FolderInMyFiles&) = delete;

  base::FilePath folder_;
  std::vector<base::FilePath> files_;
};

// Load the default set of component extensions used on ChromeOS. This should be
// done in an override of InProcessBrowserTest::SetUpOnMainThread().
void AddDefaultComponentExtensionsOnMainThread(Profile* profile);

}  // namespace test
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_
