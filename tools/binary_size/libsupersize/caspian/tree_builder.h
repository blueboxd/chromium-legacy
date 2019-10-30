// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_TREE_BUILDER_H_
#define TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_TREE_BUILDER_H_

#include "model.h"

#include <string_view>
#include <unordered_map>

namespace caspian {
class TreeBuilder {
 public:
  TreeBuilder(SizeInfo* size_info);
  ~TreeBuilder();
  void Build();
  Json::Value Open(const char* path);

 private:
  void AddFileEntry(const std::string_view source_path,
                    const std::vector<const Symbol*>& symbols);

  TreeNode* GetOrMakeParentNode(TreeNode* child_node);

  void AttachToParent(TreeNode* child, TreeNode* parent);
  std::string_view DirName(std::string_view path, char sep);

  // Merges dex method symbols into containers based on the class of the dex
  // method.
  void JoinDexMethodClasses(TreeNode* node);

  TreeNode _root;
  // TODO: A full hash table might be overkill here - could walk tree to find
  // node.
  std::unordered_map<std::string_view, TreeNode*> _parents;

  SizeInfo* _size_info = nullptr;
};  // TreeBuilder
}  // namespace caspian
#endif  // TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_TREE_BUILDER_H_
