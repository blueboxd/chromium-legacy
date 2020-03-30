// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/test/test_utils.h"

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

namespace upboarding {
namespace test {

namespace {
void SerializeEntry(const QueryTileEntry* entry, std::stringstream& out) {
  if (!entry)
    return;
  out << "entry id: " << entry->id << " query text: " << entry->query_text
      << "  display text: " << entry->display_text
      << "  accessibility_text: " << entry->accessibility_text << " \n";

  for (const auto& image : entry->image_metadatas)
    out << "image id: " << image.id << " image url: " << image.url.spec()
        << " \n";
}

}  // namespace

const std::string DebugString(const QueryTileEntry* root) {
  if (!root)
    return std::string();
  std::stringstream out;
  out << "entries detail: \n";
  std::map<std::string, std::vector<std::string>> cache;
  std::deque<const QueryTileEntry*> queue;
  queue.emplace_back(root);
  while (!queue.empty()) {
    size_t size = queue.size();
    for (size_t i = 0; i < size; i++) {
      auto* parent = queue.front();
      SerializeEntry(parent, out);
      queue.pop_front();
      for (size_t j = 0; j < parent->sub_tiles.size(); j++) {
        cache[parent->id].emplace_back(parent->sub_tiles[j]->id);
        queue.emplace_back(parent->sub_tiles[j].get());
      }
    }
  }
  out << "tree table: \n";
  for (auto& pair : cache) {
    std::string line;
    line += pair.first + " : [";
    std::sort(pair.second.begin(), pair.second.end());
    for (const auto& child : pair.second)
      line += " " + child;
    line += " ]\n";
    out << line;
  }
  return out.str();
}

}  // namespace test

}  // namespace upboarding
