// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tree_builder.h"

#include <iostream>

namespace caspian {
/** Name used by a directory created to hold symbols with no name. */
namespace {
constexpr const char* kNoName = "(No path)";
}

TreeBuilder::TreeBuilder(SizeInfo* size_info) : _size_info(size_info) {}
TreeBuilder::~TreeBuilder() = default;

void TreeBuilder::Build() {
  // Initialize tree root.
  _root.container_type = ContainerType::kDirectory;
  _root.id_path = "/";
  _parents[""] = &_root;

  // Group symbols by source path.
  std::unordered_map<std::string_view, std::vector<const Symbol*>> symbols;
  for (auto& sym : _size_info->raw_symbols) {
    std::string_view key = sym.source_path;
    if (key == nullptr) {
      key = sym.object_path;
    }
    symbols[key].push_back(&sym);
  }
  for (const auto& pair : symbols) {
    AddFileEntry(pair.first, pair.second);
  }
}

Json::Value TreeBuilder::Open(const char* path) {
  // Returns a string that can be parsed to a JS object.
  static std::string result;
  const auto node = _parents.find(path);

  if (node != _parents.end()) {
    Json::Value v;
    node->second->WriteIntoJson(&v, 1);
    return v;
  } else {
    std::cerr << "Tried to open nonexistent node with path: " << path
              << std::endl;
    exit(1);
  }
}

void TreeBuilder::AddFileEntry(const std::string_view source_path,
                               const std::vector<const Symbol*>& symbols) {
  // Creates a single file node with a child for each symbol in that file.
  TreeNode* file_node = new TreeNode();
  file_node->container_type = ContainerType::kFile;
  if (source_path.empty()) {
    file_node->id_path = kNoName;
  } else {
    file_node->id_path = source_path;
  }
  file_node->short_name_index = source_path.find_last_of('/') + 1;
  _parents[file_node->id_path] = file_node;
  // TODO: Initialize file type, source path, component

  // Create symbol nodes.
  for (const Symbol* sym : symbols) {
    TreeNode* symbol_node = new TreeNode();
    symbol_node->container_type = ContainerType::kSymbol;
    symbol_node->id_path = sym->full_name;
    symbol_node->size = sym->size;
    symbol_node->node_stats = NodeStats(
        _size_info->ShortSectionName(sym->section_name), 1, 0, sym->size);
    AttachToParent(symbol_node, file_node);
  }

  // TODO: Only add if there are unfiltered symbols in this file.
  TreeNode* orphan_node = file_node;
  while (orphan_node != &_root) {
    orphan_node = GetOrMakeParentNode(orphan_node);
  }

  JoinDexMethodClasses(file_node);
}

TreeNode* TreeBuilder::GetOrMakeParentNode(TreeNode* child_node) {
  std::string_view parent_path;
  if (child_node->id_path.empty()) {
    parent_path = kNoName;
  } else {
    parent_path = DirName(child_node->id_path, '/');
  }

  TreeNode*& parent = _parents[parent_path];
  if (parent == nullptr) {
    parent = new TreeNode();
    parent->id_path = parent_path;
    parent->short_name_index = parent_path.find_last_of('/') + 1;
    // TODO: Container type might be a component instead of a directory.
    parent->container_type = ContainerType::kDirectory;
  }
  if (child_node->parent != parent) {
    AttachToParent(child_node, parent);
  }
  return parent;
}

void TreeBuilder::AttachToParent(TreeNode* child, TreeNode* parent) {
  if (child->parent != nullptr) {
    std::cerr << "Child " << child->id_path << " already attached to parent "
              << child->parent->id_path << std::endl;
    std::cerr << "Cannot be attached to " << parent->id_path << std::endl;
    exit(1);
  }

  parent->children.push_back(child);
  child->parent = parent;

  // Update size information along tree.
  TreeNode* node = child;
  while (node->parent) {
    node->parent->size += child->size;
    node->parent->node_stats += child->node_stats;
    node = node->parent;
  }
}

std::string_view TreeBuilder::DirName(std::string_view path, char sep) {
  auto end = path.find_last_of(sep);
  if (end != std::string_view::npos) {
    std::string_view ret = path;
    ret.remove_suffix(path.size() - end);
    return ret;
  }
  return "";
}

void TreeBuilder::JoinDexMethodClasses(TreeNode* node) {
  const bool is_file_node = node->container_type == ContainerType::kFile;
  const bool has_dex =
      node->node_stats.child_stats.count(SectionId::kDex) ||
      node->node_stats.child_stats.count(SectionId::kDexMethod);
  // Don't try to merge dex symbols for catch-all symbols under (No path).
  const bool is_no_path = node->id_path == kNoName;

  if (!is_file_node || !has_dex || is_no_path || node->children.empty()) {
    return;
  }

  std::map<std::string_view, TreeNode*> java_class_containers;
  std::vector<TreeNode*> other_symbols;

  // Bucket dex symbols by their class.
  for (TreeNode* child : node->children) {
    // Unlike in .ndjson fields, Java classes loaded from .size files are just
    // the classname, such as "android.support.v7.widget.toolbar".
    // Method names contain the classname followed by the method definition,
    // like "android.support.v7.widget.toolbar void onMeasure(int, int)".
    const size_t split_index = child->id_path.find_first_of(' ');
    // No return type / field type means it's a class node.
    const bool is_class_node =
        child->id_path.find_first_of(' ', child->short_name_index) ==
        std::string_view::npos;
    const bool has_class_prefix =
        is_class_node || split_index != std::string_view::npos;

    if (has_class_prefix) {
      const std::string_view class_id_path =
          split_index == std::string_view::npos
              ? child->id_path
              : child->id_path.substr(0, split_index);

      // Strip package from the node name for classes in .java files since the
      // directory tree already shows it.
      int short_name_index = child->short_name_index;
      size_t java_idx = node->id_path.find(".java");
      if (java_idx != std::string_view::npos) {
        size_t dot_idx = class_id_path.find_last_of('.');
        short_name_index += dot_idx + 1;
      }

      TreeNode*& class_node = java_class_containers[class_id_path];
      if (class_node == nullptr) {
        class_node = new TreeNode();
        class_node->id_path = class_id_path;
        class_node->src_path = node->src_path;
        class_node->component = node->component;
        class_node->short_name_index = short_name_index;
        class_node->container_type = ContainerType::kJavaClass;
        _parents[class_node->id_path] = class_node;
      }

      // Adjust the dex method's short name so it starts after the " "
      if (split_index != std::string_view::npos) {
        child->short_name_index = split_index + 1;
      }
      child->parent = nullptr;
      AttachToParent(child, class_node);
    } else {
      other_symbols.push_back(child);
    }
  }

  node->children = other_symbols;
  for (auto& iter : java_class_containers) {
    TreeNode* container_node = iter.second;
    // Delay setting the parent until here so that `_attachToParent`
    // doesn't add method stats twice
    container_node->parent = node;
    node->children.push_back(container_node);
  }
}

}  // namespace caspian
