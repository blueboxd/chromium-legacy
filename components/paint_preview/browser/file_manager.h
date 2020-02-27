// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "url/gurl.h"

namespace paint_preview {

struct DirectoryKey {
  const std::string ascii_dirname;
};

// Manages paint preview files associated with a root directory typically the
// root directory is <profile_dir>/paint_previews/<feature>.
class FileManager {
 public:
  // Create a file manager for |root_directory|. Top level items in
  // |root_directoy| should be exclusively managed by this class. Items within
  // the subdirectories it creates can be freely modified.
  explicit FileManager(const base::FilePath& root_directory);
  ~FileManager();

  FileManager(const FileManager&) = delete;
  FileManager& operator=(const FileManager&) = delete;

  // Creates a DirectoryKey from keying material.
  // TODO(crbug/1056226): implement collision resolution. At present collisions
  // result in overwriting data.
  DirectoryKey CreateKey(const GURL& url) const;
  DirectoryKey CreateKey(uint64_t tab_id) const;

  // Get statistics about the time of creation and size of artifacts.
  size_t GetSizeOfArtifacts(const DirectoryKey& key);
  bool GetCreatedTime(const DirectoryKey& key, base::Time* created_time);
  bool GetLastModifiedTime(const DirectoryKey& key,
                           base::Time* last_modified_time);

  // Returns true if the directory for |key| exists.
  bool DirectoryExists(const DirectoryKey& key);

  // Creates or gets a subdirectory under |root_directory| for |key| and
  // assigns it to |directory|. Returns true on success. If the directory was
  // compressed then it will be uncompressed automatically.
  bool CreateOrGetDirectory(const DirectoryKey& key, base::FilePath* directory);

  // Compresses the directory associated with |url|. Returns true on success or
  // if the directory was already compressed.
  // NOTE: an empty directory or a directory containing only empty
  // files/directories will not be compressed.
  bool CompressDirectory(const DirectoryKey& key);

  // Deletes artifacts associated with |key| or |keys|.
  void DeleteArtifacts(const DirectoryKey& key);
  void DeleteArtifacts(const std::vector<DirectoryKey>& keys);

  // Deletes all stored paint previews stored in the |root_directory_|.
  void DeleteAll();

  // Serializes |proto| to the directory for |key|. Returns true on success.
  bool SerializePaintPreviewProto(const DirectoryKey& key,
                                  const PaintPreviewProto& proto);

  // Deserializes PaintPreviewProto stored in |key|. Will return nullptr if not
  // found or the proto cannot be parsed.
  std::unique_ptr<PaintPreviewProto> DeserializePaintPreviewProto(
      const DirectoryKey& key);

 private:
  enum StorageType {
    kNone = 0,
    kDirectory = 1,
    kZip = 2,
  };

  StorageType GetPathForKey(const DirectoryKey& key, base::FilePath* path);

  base::FilePath root_directory_;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_FILE_MANAGER_H_
