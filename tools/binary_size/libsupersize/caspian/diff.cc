// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/caspian/diff.h"

#include <functional>
#include <iostream>
#include <list>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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

namespace {
// Copied from /base/stl_util.h
template <class T, class Allocator, class Value>
void Erase(std::vector<T, Allocator>& container, const Value& value) {
  container.erase(std::remove(container.begin(), container.end(), value),
                  container.end());
}

std::string_view GetIdPath(const caspian::Symbol& sym) {
  return (sym.SourcePath() && *sym.SourcePath()) ? sym.SourcePath()
                                                 : sym.ObjectPath();
}

// |full_name| is costly enough to derive that we'd rather avoid it.
// Try to match on |raw_name| if possible.
SymbolMatchIndex SectionAndFullNameAndPathAndSize(const caspian::Symbol& sym) {
  return SymbolMatchIndex(sym.section_id_, sym.full_name_, GetIdPath(sym),
                          sym.Pss());
}

SymbolMatchIndex SectionAndFullNameAndPath(const caspian::Symbol& sym) {
  return SymbolMatchIndex(sym.section_id_, sym.full_name_, GetIdPath(sym),
                          0.0f);
}

// Allows signature changes (uses |Name()| rather than |FullName()|)
SymbolMatchIndex SectionAndNameAndPath(const caspian::Symbol& sym) {
  return SymbolMatchIndex(sym.section_id_, sym.Name(), GetIdPath(sym), 0.0f);
}

// Match on full name, but without path (to account for file moves)
SymbolMatchIndex SectionAndFullName(const caspian::Symbol& sym) {
  return SymbolMatchIndex(sym.section_id_, sym.full_name_, "", 0.0f);
}

int MatchSymbols(
    std::function<SymbolMatchIndex(const caspian::Symbol&)> key_func,
    std::vector<caspian::DeltaSymbol>* delta_symbols,
    std::vector<const caspian::Symbol*>* unmatched_before,
    std::vector<const caspian::Symbol*>* unmatched_after) {
  // TODO(jaspercb): Accumulate added/dropped padding by section name.
  int n_matched_symbols = 0;
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
        caspian::DeltaSymbol delta_sym(before_sym, after_sym);
        if (delta_sym.Pss() != 0.0) {
          delta_symbols->push_back(delta_sym);
        }
        before_sym = nullptr;
        after_sym = nullptr;
        n_matched_symbols++;
      }
    }
  }

  Erase(*unmatched_before, nullptr);
  Erase(*unmatched_after, nullptr);
  return n_matched_symbols;
}
}  // namespace

namespace caspian {

DeltaSizeInfo Diff(const SizeInfo* before, const SizeInfo* after) {
  DeltaSizeInfo ret(before, after);

  std::vector<const Symbol*> unmatched_before;
  for (const Symbol& sym : before->raw_symbols) {
    unmatched_before.push_back(&sym);
  }

  std::vector<const Symbol*> unmatched_after;
  for (const Symbol& sym : after->raw_symbols) {
    unmatched_after.push_back(&sym);
  }

  // Attempt several rounds to use increasingly loose matching on unmatched
  // symbols.  Any symbols still unmatched are tried in the next round.
  int step = 0;
  auto key_funcs = {SectionAndFullNameAndPathAndSize, SectionAndFullNameAndPath,
                    SectionAndNameAndPath, SectionAndFullName};
  for (const auto& key_function : key_funcs) {
    int n_matched_symbols = MatchSymbols(key_function, &ret.delta_symbols,
                                         &unmatched_before, &unmatched_after);
    std::cout << "Matched " << n_matched_symbols << " symbols in matching pass "
              << ++step << std::endl;
  }

  // Add removals or deletions for any unmatched symbols.
  for (const Symbol* after_sym : unmatched_after) {
    ret.delta_symbols.push_back(DeltaSymbol(nullptr, after_sym));
  }
  for (const Symbol* before_sym : unmatched_before) {
    ret.delta_symbols.push_back(DeltaSymbol(before_sym, nullptr));
  }
  return ret;
}
}  // namespace caspian
