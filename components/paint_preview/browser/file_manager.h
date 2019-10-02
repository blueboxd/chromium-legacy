// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace paint_preview {

// Manages paint preview files associated with a root directory (typically a
// user profile).
class FileManager {
 public:
  // Create a file manager for |root_directory|/paint_previews
  explicit FileManager(const base::FilePath& root_directory);
  ~FileManager();

  // Get statistics about the time of creation and size of artifacts.
  size_t GetSizeOfArtifactsFor(const GURL& url);
  bool GetCreatedTime(const GURL& url, base::Time* created_time);
  bool GetLastAccessedTime(const GURL& url, base::Time* last_accessed_time);

  // Creates or gets a subdirectory under |root_directory|/paint_previews/
  // for |url| and assigns it to |directory|. Returns true and modifies
  // |directory| on success.
  bool CreateOrGetDirectoryFor(const GURL& url, base::FilePath* directory);

  // Deletes artifacts associated with |urls|.
  void DeleteArtifactsFor(const std::vector<GURL>& urls);

  // Deletes all stored paint previews stored in the |profile_directory_|.
  void DeleteAll();

  // Deletes all captures with access times older than |deletion_time|. Slow and
  // blocking as it relies on base::FileEnumerator.
  void DeleteAllOlderThan(base::Time deletion_time);

 private:
  bool LastAccessedTimeInternal(const base::FilePath& path,
                                base::Time* last_accessed_time);

  base::FilePath root_directory_;

  DISALLOW_COPY_AND_ASSIGN(FileManager);
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_
