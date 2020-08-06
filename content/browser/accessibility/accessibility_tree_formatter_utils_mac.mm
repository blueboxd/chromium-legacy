// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_utils_mac.h"

namespace content {
namespace a11y {

LineIndexesMap::LineIndexesMap(const BrowserAccessibilityCocoa* cocoa_node) {
  int counter = 0;
  Build(cocoa_node, &counter);
}

LineIndexesMap::~LineIndexesMap() {}

std::string LineIndexesMap::IndexBy(
    const BrowserAccessibilityCocoa* cocoa_node) const {
  std::string line_index = ":unknown";
  if (map.find(cocoa_node) != map.end()) {
    line_index = map.at(cocoa_node);
  }
  return line_index;
}

gfx::NativeViewAccessible LineIndexesMap::NodeBy(
    const std::string& line_index) const {
  for (std::pair<const gfx::NativeViewAccessible, std::string> item : map) {
    if (item.second == line_index) {
      return item.first;
    }
  }
  return nil;
}

void LineIndexesMap::Build(const BrowserAccessibilityCocoa* cocoa_node,
                           int* counter) {
  const std::string line_index =
      std::string(1, ':') + base::NumberToString(++(*counter));
  map.insert({cocoa_node, line_index});
  for (BrowserAccessibilityCocoa* cocoa_child in [cocoa_node children]) {
    Build(cocoa_child, counter);
  }
}
}  // namespace a11y
}  // namespace content
