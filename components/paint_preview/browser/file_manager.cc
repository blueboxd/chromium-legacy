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

FileManager::FileManager(
    const base::FilePath& root_directory,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : root_directory_(root_directory), io_task_runner_(io_task_runner) {}

FileManager::~FileManager() = default;

DirectoryKey FileManager::CreateKey(const GURL& url) const {
  uint32_t hash = base::PersistentHash(url.spec());
  return DirectoryKey{base::HexEncode(&hash, sizeof(uint32_t))};
}

DirectoryKey FileManager::CreateKey(uint64_t tab_id) const {
  return DirectoryKey{base::NumberToString(tab_id)};
}

size_t FileManager::GetSizeOfArtifacts(const DirectoryKey& key) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  switch (storage_type) {
    case kDirectory: {
      return base::ComputeDirectorySize(
          root_directory_.AppendASCII(key.AsciiDirname()));
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

base::Optional<base::File::Info> FileManager::GetInfo(
    const DirectoryKey& key) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  if (storage_type == FileManager::StorageType::kNone)
    return base::nullopt;
  base::File::Info info;
  if (!base::GetFileInfo(path, &info))
    return base::nullopt;
  return info;
}

bool FileManager::DirectoryExists(const DirectoryKey& key) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath path;
  return GetPathForKey(key, &path) != StorageType::kNone;
}

bool FileManager::CaptureExists(const DirectoryKey& key) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  switch (storage_type) {
    case kDirectory:
      return base::PathExists(path.AppendASCII(kProtoName));
    case kZip:
      return true;
    case kNone:  // fallthrough;
    default:
      return false;
  }
}

base::Optional<base::FilePath> FileManager::CreateOrGetDirectory(
    const DirectoryKey& key,
    bool clear) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (clear)
    DeleteArtifactSet(key);

  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  switch (storage_type) {
    case kNone: {
      base::FilePath new_path = root_directory_.AppendASCII(key.AsciiDirname());
      base::File::Error error = base::File::FILE_OK;
      if (base::CreateDirectoryAndGetError(new_path, &error)) {
        return new_path;
      }
      DVLOG(1) << "ERROR: failed to create directory: " << path
               << " with error code " << error;
      return base::nullopt;
    }
    case kDirectory:
      return path;
    case kZip: {
      base::FilePath dst_path = root_directory_.AppendASCII(key.AsciiDirname());
      base::File::Error error = base::File::FILE_OK;
      if (!base::CreateDirectoryAndGetError(dst_path, &error)) {
        DVLOG(1) << "ERROR: failed to create directory: " << path
                 << " with error code " << error;
        return base::nullopt;
      }
      if (!zip::Unzip(path, dst_path)) {
        DVLOG(1) << "ERROR: failed to unzip: " << path << " to " << dst_path;
        return base::nullopt;
      }
      base::DeleteFileRecursively(path);
      return dst_path;
    }
    default:
      return base::nullopt;
  }
}

bool FileManager::CompressDirectory(const DirectoryKey& key) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
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

void FileManager::DeleteArtifactSet(const DirectoryKey& key) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath path;
  StorageType storage_type = GetPathForKey(key, &path);
  if (storage_type == FileManager::StorageType::kNone)
    return;
  base::DeleteFileRecursively(path);
}

void FileManager::DeleteArtifactSets(
    const std::vector<DirectoryKey>& keys) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  for (const auto& key : keys)
    DeleteArtifactSet(key);
}

void FileManager::DeleteAll() const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  base::DeleteFileRecursively(root_directory_);
}

bool FileManager::SerializePaintPreviewProto(const DirectoryKey& key,
                                             const PaintPreviewProto& proto,
                                             bool compress) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  auto path = CreateOrGetDirectory(key, false);
  if (!path.has_value())
    return false;
  return WriteProtoToFile(path->AppendASCII(kProtoName), proto) &&
         (!compress || CompressDirectory(key));
}

std::unique_ptr<PaintPreviewProto> FileManager::DeserializePaintPreviewProto(
    const DirectoryKey& key) const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  auto path = CreateOrGetDirectory(key, false);
  if (!path.has_value())
    return nullptr;
  return ReadProtoFromFile(path->AppendASCII(kProtoName));
}

base::flat_set<DirectoryKey> FileManager::ListUsedKeys() const {
  base::FileEnumerator enumerator(
      root_directory_,
      /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  std::vector<DirectoryKey> keys;
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    keys.push_back(
        DirectoryKey{name.BaseName().RemoveExtension().MaybeAsASCII()});
  }
  return base::flat_set<DirectoryKey>(std::move(keys));
}

FileManager::StorageType FileManager::GetPathForKey(
    const DirectoryKey& key,
    base::FilePath* path) const {
  base::FilePath directory_path =
      root_directory_.AppendASCII(key.AsciiDirname());
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
