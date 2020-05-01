// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Md5sum implementation for Android. In gzip mode, takes in a list of files,
// and outputs a list of Md5sums in the same order. Otherwise,

#include <stddef.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>

#include "base/base64.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/strings/string_split.h"

#include "third_party/zlib/google/compression_utils.h"

namespace {

// Only used in the gzip mode.
const char* const kFilePathDelimiter = ";";
const int kMD5HashLength = 16;

// Returns whether |path|'s MD5 was successfully written to |digest_string|.
bool MD5Sum(const std::string& path, std::string* digest_string) {
  base::ScopedFILE file(fopen(path.c_str(), "rb"));
  if (!file) {
    LOG(ERROR) << "Could not open file " << path;
    return false;
  }
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  const size_t kBufferSize = 1 << 16;
  std::unique_ptr<char[]> buf(new char[kBufferSize]);
  size_t len;
  while ((len = fread(buf.get(), 1, kBufferSize, file.get())) > 0)
    base::MD5Update(&ctx, base::StringPiece(buf.get(), len));
  if (ferror(file.get())) {
    LOG(ERROR) << "Error reading file " << path;
    return false;
  }
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  *digest_string = base::MD5DigestToBase16(digest);
  return true;
}

// Returns the set of all files contained in |files|. This handles directories
// by walking them recursively. Excludes, .svn directories and file under them.
std::vector<std::string> MakeFileSet(const char** files) {
  const std::string svn_dir_component = FILE_PATH_LITERAL("/.svn/");
  std::set<std::string> file_set;
  for (const char** file = files; *file; ++file) {
    base::FilePath file_path(*file);
    if (base::DirectoryExists(file_path)) {
      base::FileEnumerator file_enumerator(
          file_path, true /* recurse */, base::FileEnumerator::FILES);
      for (base::FilePath child, empty;
           (child = file_enumerator.Next()) != empty; ) {
        // If the path contains /.svn/, ignore it.
        if (child.value().find(svn_dir_component) == std::string::npos) {
          file_set.insert(child.value());
        }
      }
    } else {
      file_set.insert(*file);
    }
  }

  return std::vector<std::string>(file_set.begin(), file_set.end());
}

std::vector<std::string> MakeFileListFromCompressedList(const char* data) {
  std::vector<std::string> file_list;
  std::string gzipdata;
  base::Base64Decode(base::StringPiece(data), &gzipdata);
  std::string output;
  compression::GzipUncompress(gzipdata, &output);
  for (const auto& file :
       base::SplitStringPiece(output, kFilePathDelimiter, base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    file_list.push_back(file.as_string());
  }
  return file_list;
}

}  // namespace

int main(int argc, const char* argv[]) {
  bool gzip_mode = argc >= 2 && strcmp("-gz", argv[1]) == 0;
  if (argc < 2 || (gzip_mode && argc < 3)) {
    LOG(ERROR) << "Usage: md5sum <path/to/file_or_dir>... or md5sum "
               << "-gz base64-gzipped-'" << kFilePathDelimiter
               << "'-separated-files";
    return 1;
  }
  std::vector<std::string> files;
  if (gzip_mode) {
    files = MakeFileListFromCompressedList(argv[2]);
  } else {
    files = MakeFileSet(argv + 1);
  }

  bool failed = false;
  std::string digest;
  for (const auto& file : files) {
    if (!MD5Sum(file, &digest)) {
      failed = true;
      continue;
    }
    if (gzip_mode) {
      std::cout << digest.substr(0, kMD5HashLength) << std::endl;
    } else {
      std::cout << digest << "  " << file << std::endl;
    }
  }
  return failed;
}
