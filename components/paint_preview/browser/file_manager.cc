// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/file_manager.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace paint_preview {

namespace {

constexpr char kPaintPreviewDirname[] = "paint_preview";

std::string HashToHex(const GURL& url) {
  uint32_t hash = base::PersistentHash(url.spec());
  return base::HexEncode(&hash, sizeof(uint32_t));
}

}  // namespace

FileManager::FileManager(const base::FilePath& root_directory)
    : root_directory_(root_directory.AppendASCII(kPaintPreviewDirname)) {}
FileManager::~FileManager() = default;

size_t FileManager::GetSizeOfArtifactsFor(const GURL& url) {
  return base::ComputeDirectorySize(
      root_directory_.AppendASCII(HashToHex(url)));
}

bool FileManager::GetCreatedTime(const GURL& url, base::Time* created_time) {
  base::File::Info info;
  if (!base::GetFileInfo(root_directory_.AppendASCII(HashToHex(url)), &info))
    return false;
  *created_time = info.creation_time;
  return true;
}

bool FileManager::GetLastAccessedTime(const GURL& url,
                                      base::Time* last_accessed_time) {
  return LastAccessedTimeInternal(root_directory_.AppendASCII(HashToHex(url)),
                                  last_accessed_time);
}

bool FileManager::CreateOrGetDirectoryFor(const GURL& url,
                                          base::FilePath* directory) {
  base::FilePath path = root_directory_.AppendASCII(HashToHex(url));
  base::File::Error error = base::File::FILE_OK;
  if (base::CreateDirectoryAndGetError(path, &error)) {
    *directory = path;
    return true;
  }
  DLOG(ERROR) << "Error failed to create directory: " << path
              << " with error code " << error;
  return false;
}

void FileManager::DeleteArtifactsFor(const std::vector<GURL>& urls) {
  for (const auto& url : urls)
    base::DeleteFile(root_directory_.AppendASCII(HashToHex(url)), true);
}

void FileManager::DeleteAll() {
  base::DeleteFile(root_directory_, true);
}

void FileManager::DeleteAllOlderThan(base::Time deletion_time) {
  std::vector<base::FilePath> dirs_to_delete;
  base::FileEnumerator enumerator(root_directory_, false,
                                  base::FileEnumerator::DIRECTORIES);
  base::Time last_accessed_time;
  for (base::FilePath dir = enumerator.Next(); !dir.empty();
       dir = enumerator.Next()) {
    if (!LastAccessedTimeInternal(dir, &last_accessed_time))
      continue;
    if (last_accessed_time < deletion_time)
      dirs_to_delete.push_back(dir);
  }
  for (const auto& dir : dirs_to_delete)
    base::DeleteFile(dir, true);
}

bool FileManager::LastAccessedTimeInternal(const base::FilePath& path,
                                           base::Time* last_accessed_time) {
  base::File::Info info;
  if (!base::GetFileInfo(path, &info))
    return false;
  *last_accessed_time = info.last_accessed;
  return true;
}

}  // namespace paint_preview
