// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_MODEL_H_
#define TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_MODEL_H_

#include <stdlib.h>

#include <deque>
#include <vector>

#include "third_party/jsoncpp/source/include/json/json.h"

// Copied from representation in tools/binary_size/libsupersize/models.py

namespace caspian {

struct Symbol {
  Symbol();
  Symbol(const Symbol& other);

  int32_t address = 0;
  int32_t size = 0;
  int32_t flags = 0;
  int32_t padding = 0;
  const char* full_name = nullptr;
  const char* section_name = nullptr;
  const char* object_path = nullptr;
  const char* source_path = nullptr;
  const char* component = nullptr;
  std::vector<Symbol*>* aliases = nullptr;
};

struct SizeInfo {
  SizeInfo();
  ~SizeInfo();
  SizeInfo(const SizeInfo& other) = delete;
  SizeInfo& operator=(const SizeInfo& other) = delete;

  std::vector<caspian::Symbol> raw_symbols;
  Json::Value metadata;

  // Entries in |raw_symbols| hold pointers to this data.
  std::vector<const char*> object_paths;
  std::vector<const char*> source_paths;
  std::vector<const char*> components;
  std::vector<const char*> section_names;
  std::vector<char> raw_decompressed;

  // A container for each symbol group.
  std::deque<std::vector<Symbol*>> alias_groups;
};

}  // namespace caspian
#endif  // TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_MODEL_H_
