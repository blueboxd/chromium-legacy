// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_MODEL_H_
#define TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_MODEL_H_

#include <stdlib.h>

#include <deque>
#include <map>
#include <vector>

#include "third_party/jsoncpp/source/include/json/json.h"

// Copied from representation in tools/binary_size/libsupersize/models.py

namespace caspian {

enum class ContainerType : char {
  kSymbol = '\0',
  kDirectory = 'D',
  kComponent = 'C',
  kFile = 'F',
  kJavaClass = 'J',
};

enum class SectionId : char {
  // kNone is unused except for default-initializing in containers
  kNone = '\0',
  kBss = 'b',
  kData = 'd',
  kDataRelRo = 'R',
  kDex = 'x',
  kDexMethod = 'm',
  kOther = 'o',
  kRoData = 'r',
  kText = 't',
  kPakNontranslated = 'P',
  kPakTranslations = 'p',
};

struct Symbol {
  Symbol();
  Symbol(const Symbol& other);

  int32_t address = 0;
  int32_t size = 0;
  int32_t flags = 0;
  int32_t padding = 0;
  // Pointers into SizeInfo->raw_decompressed;
  const char* section_name = nullptr;
  const char* full_name = nullptr;
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
  SectionId ShortSectionName(const char* section_name);

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

struct Stat {
  int32_t count = 0;
  int32_t highlight = 0;
  int32_t size = 0;

  void operator+=(const Stat& other) {
    count += other.count;
    highlight += other.highlight;
    size += other.size;
  }
};

struct NodeStats {
  NodeStats();
  ~NodeStats();
  NodeStats(SectionId section, int32_t count, int32_t highlight, int32_t size);
  void WriteIntoJson(Json::Value* out) const;
  NodeStats& operator+=(const NodeStats& other);
  SectionId ComputeBiggestSection() const;

  std::map<SectionId, Stat> child_stats;
};

struct TreeNode {
  TreeNode();
  ~TreeNode();
  void WriteIntoJson(Json::Value* out, int depth);

  std::string_view id_path;
  const char* src_path = nullptr;
  const char* component = nullptr;
  int32_t size = 0;
  NodeStats node_stats;
  int32_t flags = 0;
  int32_t short_name_index = 0;

  /*
    type,
    numAliases,
    childStats,
  */

  ContainerType container_type = ContainerType::kSymbol;

  std::vector<TreeNode*> children;
  TreeNode* parent = nullptr;
  Symbol* symbol = nullptr;
};

}  // namespace caspian
#endif  // TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_MODEL_H_
