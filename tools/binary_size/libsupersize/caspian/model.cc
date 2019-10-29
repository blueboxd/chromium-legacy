// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/caspian/model.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "tools/binary_size/libsupersize/caspian/file_format.h"

using namespace caspian;

Symbol::Symbol() = default;
Symbol::Symbol(const Symbol& other) = default;

TreeNode::TreeNode() = default;
TreeNode::~TreeNode() = default;

SizeInfo::SizeInfo() = default;
SizeInfo::~SizeInfo() = default;

SectionId SizeInfo::ShortSectionName(const char* section_name) {
  static std::map<const char*, SectionId> short_section_name_cache;
  SectionId& ret = short_section_name_cache[section_name];
  if (ret == SectionId::kNone) {
    if (!strcmp(section_name, ".text")) {
      ret = SectionId::kText;
    } else if (!strcmp(section_name, ".dex")) {
      ret = SectionId::kDex;
    } else if (!strcmp(section_name, ".dex.method")) {
      ret = SectionId::kDexMethod;
    } else if (!strcmp(section_name, ".other")) {
      ret = SectionId::kOther;
    } else if (!strcmp(section_name, ".rodata")) {
      ret = SectionId::kRoData;
    } else if (!strcmp(section_name, ".data")) {
      ret = SectionId::kData;
    } else if (!strcmp(section_name, ".data.rel.ro")) {
      ret = SectionId::kDataRelRo;
    } else if (!strcmp(section_name, ".bss")) {
      ret = SectionId::kBss;
    } else if (!strcmp(section_name, ".bss.rel.ro")) {
      ret = SectionId::kBss;
    } else if (!strcmp(section_name, ".pak.nontranslated")) {
      ret = SectionId::kPakNontranslated;
    } else if (!strcmp(section_name, ".pak.translations")) {
      ret = SectionId::kPakTranslations;
    } else {
      std::cerr << "Attributing unrecognized section name to .other: "
                << section_name << std::endl;
      ret = SectionId::kOther;
    }
  }
  return ret;
}

void TreeNode::WriteIntoJson(Json::Value* out, int depth) {
  (*out)["idPath"] = std::string(this->id_path);
  (*out)["shortNameIndex"] = this->short_name_index;
  // TODO: Put correct information.
  std::string type;
  if (container_type != ContainerType::kSymbol) {
    type += static_cast<char>(container_type);
  }
  SectionId biggest_section = this->node_stats.ComputeBiggestSection();
  type += static_cast<char>(biggest_section);
  (*out)["type"] = type;

  (*out)["size"] = this->size;
  (*out)["flags"] = this->flags;
  this->node_stats.WriteIntoJson(&(*out)["childStats"]);
  if (depth < 0 && this->children.size() > 1) {
    (*out)["children"] = Json::Value();  // null
  } else {
    (*out)["children"] = Json::Value(Json::arrayValue);
    // Reorder children for output.
    // TODO: Support additional compare functions.
    auto compare_func = [](const TreeNode* const& l,
                           const TreeNode* const& r) -> bool {
      return l->size > r->size;
    };
    std::sort(this->children.begin(), this->children.end(), compare_func);
    for (unsigned int i = 0; i < this->children.size(); i++) {
      this->children[i]->WriteIntoJson(&(*out)["children"][i], depth - 1);
    }
  }
}

NodeStats::NodeStats() = default;
NodeStats::~NodeStats() = default;

NodeStats::NodeStats(SectionId sectionId,
                     int32_t count,
                     int32_t highlight,
                     int32_t size) {
  child_stats[sectionId] = {count, highlight, size};
}

void NodeStats::WriteIntoJson(Json::Value* out) const {
  (*out) = Json::Value(Json::objectValue);
  for (const auto kv : this->child_stats) {
    const std::string sectionId = std::string(1, static_cast<char>(kv.first));
    const Stat stats = kv.second;
    (*out)[sectionId] = Json::Value(Json::objectValue);
    (*out)[sectionId]["size"] = stats.size;
    (*out)[sectionId]["count"] = stats.count;
    (*out)[sectionId]["highlight"] = stats.highlight;
  }
}

NodeStats& NodeStats::operator+=(const NodeStats& other) {
  for (const auto& it : other.child_stats) {
    child_stats[it.first] += it.second;
  }
  return *this;
}

SectionId NodeStats::ComputeBiggestSection() const {
  SectionId ret = SectionId::kNone;
  int32_t max = 0;
  for (auto& pair : child_stats) {
    if (pair.second.size > max) {
      ret = pair.first;
      max = pair.second.size;
    }
  }
  return ret;
}
