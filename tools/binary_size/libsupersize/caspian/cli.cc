// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Command-line interface for checking the integrity of .size files.
// Usage: cli (path to .size file)

#include <stdlib.h>

#include <algorithm>
#include <fstream>
#include <iostream>

#include "tools/binary_size/libsupersize/caspian/file_format.h"
#include "tools/binary_size/libsupersize/caspian/model.h"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: cli (path to .size file)" << std::endl;
    exit(1);
  }
  std::ifstream ifs(argv[1], std::ifstream::in);
  if (!ifs.good()) {
    std::cerr << "Unable to open file: " << argv[1] << std::endl;
    exit(1);
  }
  std::string compressed((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
  caspian::SizeInfo info;
  caspian::ParseSizeInfo(compressed.data(), compressed.size(), &info);

  size_t max_aliases = 0;
  for (auto& s : info.raw_symbols) {
    if (s.aliases) {
      max_aliases = std::max(max_aliases, s.aliases->size());
      // What a wonderful O(n^2) loop
      for (auto* ss : *s.aliases) {
        if (ss->aliases != s.aliases) {
          std::cerr << "Not all symbols in alias group had same alias count"
                    << std::endl;
          exit(1);
        }
      }
    }
  }
  std::cout << "Largest number of aliases: " << max_aliases << std::endl;
  return 0;
}
