// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/caspian/model.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <string>
#include <unordered_map>

#include "tools/binary_size/libsupersize/caspian/file_format.h"

namespace {
struct SymbolMatchIndex {
  SymbolMatchIndex() {}
  SymbolMatchIndex(caspian::SectionId id,
                   std::string_view name,
                   std::string_view path,
                   int size_without_padding)
      : nonempty(true),
        id(id),
        name(name),
        path(path),
        size_without_padding(size_without_padding) {
    this->name = name;
  }

  operator bool() const { return nonempty; }

  bool operator==(const SymbolMatchIndex& other) const {
    return id == other.id && name == other.name && path == other.path &&
           size_without_padding == other.size_without_padding;
  }

  bool nonempty = false;
  caspian::SectionId id;
  std::string_view name;
  std::string_view path;
  int size_without_padding;
};
}  // namespace

namespace std {
template <>
struct hash<SymbolMatchIndex> {
  static constexpr size_t kPrime1 = 105929;
  static constexpr size_t kPrime2 = 8543;
  size_t operator()(const SymbolMatchIndex& k) const {
    return ((kPrime1 * static_cast<size_t>(k.id)) ^
            hash<string_view>()(k.name) ^ hash<string_view>()(k.path) ^
            (kPrime2 * k.size_without_padding));
  }
};
}  // namespace std

namespace caspian {

Symbol::Symbol() = default;
Symbol::Symbol(const Symbol& other) = default;
Symbol& Symbol::operator=(const Symbol& other) = default;

Symbol Symbol::DiffSymbolFrom(const Symbol* before_sym,
                              const Symbol* after_sym) {
  Symbol ret;
  if (after_sym) {
    ret = *after_sym;
  } else if (before_sym) {
    ret = *before_sym;
  }

  if (before_sym && after_sym) {
    ret.size = after_sym->size - before_sym->size;
    ret.pss = after_sym->pss - before_sym->pss;
  } else if (before_sym) {
    ret.size = -before_sym->size;
    ret.pss = -before_sym->pss;
  }

  return ret;
}

TreeNode::TreeNode() = default;
TreeNode::~TreeNode() {
  // TODO(jaspercb): Could use custom allocator to delete all nodes in one go.
  for (TreeNode* child : children) {
    delete child;
  }
}

std::ostream& operator<<(std::ostream& os, const Symbol& sym) {
  return os << "Symbol(full_name=" << sym.full_name
            << ", section=" << static_cast<char>(sym.sectionId)
            << ", address=" << sym.address << ", size=" << sym.size
            << ", flags=" << sym.flags << ", padding=" << sym.padding << ")";
}

BaseSizeInfo::BaseSizeInfo() = default;
BaseSizeInfo::~BaseSizeInfo() = default;

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

namespace {
// Copied from /base/stl_util.h
template <class T, class Allocator, class Value>
void Erase(std::vector<T, Allocator>& container, const Value& value) {
  container.erase(std::remove(container.begin(), container.end(), value),
                  container.end());
}

std::string_view GetIdPath(const Symbol& sym) {
  return (sym.source_path && *sym.source_path) ? sym.source_path
                                               : sym.object_path;
}

SymbolMatchIndex PerfectMatch(const Symbol& sym) {
  return SymbolMatchIndex(sym.sectionId, sym.full_name, GetIdPath(sym),
                          sym.pss);
}

SymbolMatchIndex PerfectMatchExceptSize(const Symbol& sym) {
  return SymbolMatchIndex(sym.sectionId, sym.full_name, GetIdPath(sym), 0.0f);
}

void MatchSymbols(std::function<SymbolMatchIndex(const Symbol&)> key_func,
                  std::vector<caspian::Symbol>* delta_symbols,
                  std::vector<const caspian::Symbol*>* unmatched_before,
                  std::vector<const caspian::Symbol*>* unmatched_after) {
  // TODO(jaspercb): Accumulate added/dropped padding by section name.
  std::unordered_map<SymbolMatchIndex,
                     std::list<std::reference_wrapper<const caspian::Symbol*>>>
      before_symbols_by_key;
  for (const caspian::Symbol*& before_sym : *unmatched_before) {
    SymbolMatchIndex key = key_func(*before_sym);
    if (key) {
      before_symbols_by_key[key].emplace_back(before_sym);
    }
  }

  for (const caspian::Symbol*& after_sym : *unmatched_after) {
    SymbolMatchIndex key = key_func(*after_sym);
    if (key) {
      const auto& found = before_symbols_by_key.find(key);
      if (found != before_symbols_by_key.end() && found->second.size()) {
        const caspian::Symbol*& before_sym = found->second.front().get();
        found->second.pop_front();
        delta_symbols->push_back(Symbol::DiffSymbolFrom(before_sym, after_sym));
        before_sym = nullptr;
        after_sym = nullptr;
      }
    }
  }

  Erase(*unmatched_before, nullptr);
  Erase(*unmatched_after, nullptr);
}
}  // namespace

DiffSizeInfo::DiffSizeInfo(SizeInfo* before, SizeInfo* after)
    : before(before), after(after) {
  // TODO(jaspercb): It's okay to do a first pass where we match on the raw
  // name (from the .size file), but anything left over after that first step
  // (hopefully <2k objects) needs to consider the derived full_name. Should
  // do this lazily for efficiency - name derivation is costly.

  std::vector<const caspian::Symbol*> unmatched_before;
  for (const caspian::Symbol& sym : before->raw_symbols) {
    unmatched_before.push_back(&sym);
  }

  std::vector<const caspian::Symbol*> unmatched_after;
  for (const caspian::Symbol& sym : after->raw_symbols) {
    unmatched_after.push_back(&sym);
  }

  // Attempt several rounds to use increasingly loose matching on unmatched
  // symbols.  Any symbols still unmatched are tried in the next round.
  for (const auto& key_function : {PerfectMatch, PerfectMatchExceptSize}) {
    MatchSymbols(key_function, &raw_symbols, &unmatched_before,
                 &unmatched_after);
  }

  // Add removals or deletions for any unmatched symbols.
  for (const caspian::Symbol* after_sym : unmatched_after) {
    raw_symbols.push_back(Symbol::DiffSymbolFrom(nullptr, after_sym));
  }
  for (const caspian::Symbol* before_sym : unmatched_before) {
    raw_symbols.push_back(Symbol::DiffSymbolFrom(before_sym, nullptr));
  }
}

DiffSizeInfo::~DiffSizeInfo() {}

void TreeNode::WriteIntoJson(Json::Value* out, int depth) {
  (*out)["idPath"] = std::string(this->id_path);
  (*out)["shortNameIndex"] = this->short_name_index;
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
      return abs(l->size) > abs(r->size);
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
                     float size) {
  child_stats[sectionId] = {count, size};
}

void NodeStats::WriteIntoJson(Json::Value* out) const {
  (*out) = Json::Value(Json::objectValue);
  for (const auto kv : this->child_stats) {
    const std::string sectionId = std::string(1, static_cast<char>(kv.first));
    const Stat stats = kv.second;
    (*out)[sectionId] = Json::Value(Json::objectValue);
    (*out)[sectionId]["size"] = stats.size;
    (*out)[sectionId]["count"] = stats.count;
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
  float max = 0.0f;
  for (auto& pair : child_stats) {
    if (abs(pair.second.size) > max) {
      ret = pair.first;
      max = abs(pair.second.size);
    }
  }
  return ret;
}

}  // namespace caspian
