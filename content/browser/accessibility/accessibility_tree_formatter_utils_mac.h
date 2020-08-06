// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_MAC_H_

#include "content/browser/accessibility/browser_accessibility_cocoa.h"

namespace content {
namespace a11y {

/**
 * Converts cocoa node object to a line index in the formatted accessibility
 * tree, the node is placed at, and vice versa.
 */
class LineIndexesMap {
 public:
  LineIndexesMap(const BrowserAccessibilityCocoa* cocoa_node);
  ~LineIndexesMap();

  std::string IndexBy(const BrowserAccessibilityCocoa* cocoa_node) const;
  gfx::NativeViewAccessible NodeBy(const std::string& index) const;

 private:
  void Build(const BrowserAccessibilityCocoa* cocoa_node, int* counter);

  std::map<const gfx::NativeViewAccessible, std::string> map;
};

}  // namespace a11y
}  // namespace content

#endif
