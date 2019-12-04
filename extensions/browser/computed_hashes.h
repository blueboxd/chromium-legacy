// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_COMPUTED_HASHES_H_
#define EXTENSIONS_BROWSER_COMPUTED_HASHES_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"

namespace base {
class FilePath;
}

namespace extensions {

using IsCancelledCallback = base::RepeatingCallback<bool(void)>;
using ShouldComputeHashesCallback =
    base::RepeatingCallback<bool(const base::FilePath& relative_path)>;

// A class for storage and serialization of a set of SHA256 block hashes
// computed over the files inside an extension.
class ComputedHashes {
 public:
  using HashInfo = std::pair<int, std::vector<std::string>>;
  using Data = std::map<base::FilePath, HashInfo>;

  explicit ComputedHashes(Data&& data);
  ComputedHashes(const ComputedHashes&) = delete;
  ComputedHashes& operator=(const ComputedHashes&) = delete;
  ComputedHashes(ComputedHashes&&);
  ComputedHashes& operator=(ComputedHashes&&);
  ~ComputedHashes();

  // Reads computed hashes from the computed_hashes.json file, returns nullopt
  // upon any failure.
  static base::Optional<ComputedHashes> CreateFromFile(
      const base::FilePath& path);

  // Computes hashes for files in |extension_root|. Returns nullopt upon any
  // failure. Callback |should_compute_hashes_for| is used to determine whether
  // we need hashes for a resource or not.
  // TODO(https://crbug.com/796395#c24) To support per-file block size instead
  // of passing |block_size| as an argument make callback
  // |should_compute_hashes_for| return optional<int>: nullopt if hashes are not
  // needed for this file, block size for this file otherwise.
  static base::Optional<ComputedHashes::Data> Compute(
      const base::FilePath& extension_root,
      int block_size,
      const IsCancelledCallback& is_cancelled,
      const ShouldComputeHashesCallback& should_compute_hashes_for_resource);

  // Saves computed hashes to given file, returns false upon any failure (and
  // true on success).
  bool WriteToFile(const base::FilePath& path) const;

  // Gets hash info for |relative_path|. The block size and hashes for
  // |relative_path| will be copied into the out parameters. Returns false if
  // resource was not found (and true on success).
  bool GetHashes(const base::FilePath& relative_path,
                 int* block_size,
                 std::vector<std::string>* hashes) const;

  // Returns the SHA256 hash of each |block_size| chunk in |contents|.
  static std::vector<std::string> GetHashesForContent(
      const std::string& contents,
      size_t block_size);

 private:
  // Builds hashes for one resource and checks them against
  // verified_contents.json if needed. Returns nullopt if nothing should be
  // added to computed_hashes.json for this resource.
  static base::Optional<std::vector<std::string>> ComputeAndCheckResourceHash(
      const base::FilePath& full_path,
      const base::FilePath& relative_unix_path,
      int block_size);

  Data data_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_COMPUTED_HASHES_H_
