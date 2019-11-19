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

#include "base/optional.h"

namespace base {
class FilePath;
}

namespace extensions {

// A class for storage and serialization of a set of SHA256 block hashes
// computed over the files inside an extension.
class ComputedHashes {
 public:
  using HashInfo = std::pair<int, std::vector<std::string>>;

  // While |ComputedHashes| itself is a read-only view for the hashes, this is a
  // subclass for modifying (eg. while computing hashes for the first time).
  class Data {
   public:
    Data();
    ~Data();
    Data(const Data&) = delete;
    Data& operator=(const Data&) = delete;
    Data(Data&&);
    Data& operator=(Data&&);

    // Adds hashes for |relative_path|. Should not be called more than once for
    // a given |relative_path|.
    void AddHashes(const base::FilePath& relative_path,
                   int block_size,
                   std::vector<std::string> hashes);

   private:
    // Map of relative path to hash info (block size, hashes).
    std::map<base::FilePath, HashInfo> data_;

    friend class ComputedHashes;
  };

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
  Data data_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_COMPUTED_HASHES_H_
