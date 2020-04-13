// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/test/test_utils.h"

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace upboarding {
namespace test {

namespace {

const char kTimeStr[] = "03/18/20 01:00:00 AM";

void SerializeEntry(const QueryTileEntry* entry, std::stringstream& out) {
  if (!entry)
    return;
  out << "entry id: " << entry->id << " query text: " << entry->query_text
      << "  display text: " << entry->display_text
      << "  accessibility_text: " << entry->accessibility_text << " \n";

  for (const auto& image : entry->image_metadatas)
    out << "image id: " << image.id
        << " image url: " << image.url.possibly_invalid_spec() << " \n";
}

}  // namespace

const std::string DebugString(const QueryTileEntry* root) {
  if (!root)
    return std::string();
  std::stringstream out;
  out << "Entries detail: \n";
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
  out << "Tree table: \n";
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

const std::string DebugString(const TileGroup* group) {
  if (!group)
    return std::string();
  std::stringstream out;
  out << "Group detail: \n";
  out << "id: " << group->id << "locale: " << group->locale
      << " last_updated_ts: " << group->last_updated_ts << " \n";
  for (const auto& tile : group->tiles) {
    out << DebugString(tile.get());
  }
  return out.str();
}

void ResetTestEntry(QueryTileEntry* entry) {
  entry->id = "test-guid";
  entry->query_text = "test query str";
  entry->display_text = "test display text";
  entry->accessibility_text = "read this test display text";
  entry->image_metadatas.clear();
  entry->image_metadatas.emplace_back("image-test-id-1",
                                      GURL("http://www.example.com"));
  entry->image_metadatas.emplace_back("image-test-id-2",
                                      GURL("http://www.fakeurl.com"));

  auto entry1 = std::make_unique<QueryTileEntry>();
  entry1->id = "test-guid-001";
  auto entry2 = std::make_unique<QueryTileEntry>();
  entry2->id = "test-guid-002";
  auto entry3 = std::make_unique<QueryTileEntry>();
  entry3->id = "test-guid-003";
  entry1->sub_tiles.clear();
  entry1->sub_tiles.emplace_back(std::move(entry3));
  entry->sub_tiles.clear();
  entry->sub_tiles.emplace_back(std::move(entry1));
  entry->sub_tiles.emplace_back(std::move(entry2));
}

void ResetTestGroup(TileGroup* group) {
  group->id = "group_guid";
  group->locale = "en-US";
  DCHECK(base::Time::FromString(kTimeStr, &group->last_updated_ts));
  group->tiles.clear();
  auto test_entry_1 = std::make_unique<QueryTileEntry>();
  ResetTestEntry(test_entry_1.get());
  auto test_entry_2 = std::make_unique<QueryTileEntry>();
  test_entry_2->id = "test_entry_id_2";
  auto test_entry_3 = std::make_unique<QueryTileEntry>();
  ResetTestEntry(test_entry_3.get());
  test_entry_3->id = "test_entry_id_3";
  auto test_entry_4 = std::make_unique<QueryTileEntry>();
  test_entry_4->id = "test_entry_id_4";
  test_entry_1->sub_tiles.emplace_back(std::move(test_entry_2));
  test_entry_1->sub_tiles.emplace_back(std::move(test_entry_3));
  group->tiles.emplace_back(std::move(test_entry_1));
  group->tiles.emplace_back(std::move(test_entry_4));
}

}  // namespace test

}  // namespace upboarding
