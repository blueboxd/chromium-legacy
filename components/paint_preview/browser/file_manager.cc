// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/file_manager.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "components/paint_preview/common/file_utils.h"
#include "third_party/zlib/google/zip.h"

namespace paint_preview {

namespace {

constexpr char kProtoName[] = "proto.pb";
constexpr char kZipExt[] = ".zip";

}  // namespace

FileManager::FileManager(const base::FilePath& root_directory)
    : root_directory_(root_directory) {}

FileManager::~FileManager() = default;

DirectoryKey FileManager::CreateKey(const GURL& url) const {
  uint32_t hash = base::PersistentHash(url.spec());
  return DirectoryKey{base::HexEncode(&hash, sizeof(uint32_t))};
}

DirectoryKey FileManager::CreateKey(uint64_t tab_id) const {
  return DirectoryKey{base::NumberToString(tab_id)};
}

size_t FileManager::GetSizeOfArtifacts(const DirectoryKey& key) {
  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  switch (storage_type) {
    case kDirectory: {
      return base::ComputeDirectorySize(
          root_directory_.AppendASCII(key.ascii_dirname));
    }
    case kZip: {
      int64_t file_size = 0;
      if (!base::GetFileSize(path, &file_size) || file_size < 0)
        return 0;
      return file_size;
    }
    case kNone:  // fallthrough
    default:
      return 0;
  }
}

bool FileManager::GetCreatedTime(const DirectoryKey& key,
                                 base::Time* created_time) {
  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  if (storage_type == FileManager::StorageType::kNone)
    return false;
  base::File::Info info;
  if (!base::GetFileInfo(path, &info))
    return false;
  *created_time = info.creation_time;
  return true;
}

bool FileManager::GetLastModifiedTime(const DirectoryKey& key,
                                      base::Time* last_modified_time) {
  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  if (storage_type == FileManager::StorageType::kNone)
    return false;
  base::File::Info info;
  if (!base::GetFileInfo(path, &info))
    return false;
  *last_modified_time = info.last_modified;
  return true;
}

bool FileManager::DirectoryExists(const DirectoryKey& key) {
  base::FilePath path;
  return GetPathForKey(key, &path) != StorageType::kNone;
}

bool FileManager::CreateOrGetDirectory(const DirectoryKey& key,
                                       base::FilePath* directory) {
  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  switch (storage_type) {
    case kNone: {
      base::FilePath new_path = root_directory_.AppendASCII(key.ascii_dirname);
      base::File::Error error = base::File::FILE_OK;
      if (base::CreateDirectoryAndGetError(new_path, &error)) {
        *directory = new_path;
        return true;
      }
      DVLOG(1) << "ERROR: failed to create directory: " << path
               << " with error code " << error;
      return false;
    }
    case kDirectory: {
      *directory = path;
      return true;
    }
    case kZip: {
      base::FilePath dst_path = root_directory_.AppendASCII(key.ascii_dirname);
      base::File::Error error = base::File::FILE_OK;
      if (!base::CreateDirectoryAndGetError(dst_path, &error)) {
        DVLOG(1) << "ERROR: failed to create directory: " << path
                 << " with error code " << error;
        return false;
      }
      if (!zip::Unzip(path, dst_path)) {
        DVLOG(1) << "ERROR: failed to unzip: " << path << " to " << dst_path;
        return false;
      }
      base::DeleteFileRecursively(path);
      *directory = dst_path;
      return true;
    }
    default:
      return false;
  }
}

bool FileManager::CompressDirectory(const DirectoryKey& key) {
  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  switch (storage_type) {
    case kDirectory: {
      // If there are no files in the directory, zip will succeed, but unzip
      // will not. Thus don't compress since there is no point.
      if (!base::ComputeDirectorySize(path))
        return false;
      base::FilePath dst_path = path.AddExtensionASCII(kZipExt);
      if (!zip::Zip(path, dst_path, /* hidden files */ true))
        return false;
      base::DeleteFileRecursively(path);
      return true;
    }
    case kZip:
      return true;
    case kNone:  // fallthrough
    default:
      return false;
  }
}

void FileManager::DeleteArtifacts(const DirectoryKey& key) {
  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  if (storage_type == FileManager::StorageType::kNone)
    return;
  base::DeleteFileRecursively(path);
}

void FileManager::DeleteArtifacts(const std::vector<DirectoryKey>& keys) {
  for (const auto& key : keys)
    DeleteArtifacts(key);
}

void FileManager::DeleteAll() {
  base::DeleteFileRecursively(root_directory_);
}

bool FileManager::SerializePaintPreviewProto(const DirectoryKey& key,
                                             const PaintPreviewProto& proto) {
  base::FilePath path;
  if (!CreateOrGetDirectory(key, &path))
    return false;
  return WriteProtoToFile(path.AppendASCII(kProtoName), proto);
}

std::unique_ptr<PaintPreviewProto> FileManager::DeserializePaintPreviewProto(
    const DirectoryKey& key) {
  base::FilePath path;
  if (!CreateOrGetDirectory(key, &path))
    return nullptr;
  return ReadProtoFromFile(path.AppendASCII(kProtoName));
}

FileManager::StorageType FileManager::GetPathForKey(const DirectoryKey& key,
                                                    base::FilePath* path) {
  base::FilePath directory_path =
      root_directory_.AppendASCII(key.ascii_dirname);
  if (base::PathExists(directory_path)) {
    *path = directory_path;
    return kDirectory;
  }
  base::FilePath zip_path = directory_path.AddExtensionASCII(kZipExt);
  if (base::PathExists(zip_path)) {
    *path = zip_path;
    return kZip;
  }
  return kNone;
}

}  // namespace paint_preview
