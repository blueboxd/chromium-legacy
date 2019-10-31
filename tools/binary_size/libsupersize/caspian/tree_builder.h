// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_TREE_BUILDER_H_
#define TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_TREE_BUILDER_H_

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "tools/binary_size/libsupersize/caspian/model.h"

namespace caspian {
class TreeBuilder {
 public:
  TreeBuilder(SizeInfo* size_info,
              bool group_by_component,
              std::vector<std::function<bool(const Symbol&)>> filters);
  ~TreeBuilder();
  void Build();
  Json::Value Open(const char* path);

 private:
  void AddFileEntry(const std::string_view source_path,
                    const std::vector<const Symbol*>& symbols);

  TreeNode* GetOrMakeParentNode(TreeNode* child_node);

  void AttachToParent(TreeNode* child, TreeNode* parent);

  ContainerType ContainerTypeFromChild(std::string_view child_id_path) const;

  bool ShouldIncludeSymbol(const Symbol& symbol) const;

  // Merges dex method symbols into containers based on the class of the dex
  // method.
  void JoinDexMethodClasses(TreeNode* node);

  TreeNode root_;
  // TODO: A full hash table might be overkill here - could walk tree to find
  // node.
  std::unordered_map<std::string_view, TreeNode*> _parents;

  SizeInfo* size_info_ = nullptr;
  // Contained TreeNode hold lightweight string_views to fields in SizeInfo.
  // If grouping by component, this isn't possible: TreeNode id_paths are not
  // substrings of SizeInfo-owned strings. In that case, the strings are stored
  // in |owned_strings_|.
  // Deque is used for stability, to support string_view.
  std::deque<std::string> owned_strings_;
  bool group_by_component_;
  // The current path separator: '>' if grouping by component, '/' otherwise.
  // Note that we split paths on '/' no matter the value of separator, since
  // when grouping by component, paths look like Component>path/to/file.
  char sep_;
  std::vector<std::function<bool(const Symbol&)>> filters_;
};  // TreeBuilder
}  // namespace caspian
#endif  // TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_TREE_BUILDER_H_
