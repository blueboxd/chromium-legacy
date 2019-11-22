// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/caspian/tree_builder.h"

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/binary_size/libsupersize/caspian/model.h"

namespace caspian {
namespace {

void MakeSymbol(SizeInfo* info,
                SectionId section_id,
                int32_t size,
                const char* path,
                const char* component,
                std::string_view name = "") {
  static std::deque<std::string> symbol_names;
  if (name.empty()) {
    symbol_names.push_back(std::string());
    std::string& s = symbol_names.back();
    s += static_cast<char>(section_id);
    s += "_";
    s += std::to_string(size);
    s += "A";
    name = s;
  }
  Symbol sym;
  sym.full_name_ = name;
  sym.section_id_ = section_id;
  sym.size_ = size;
  sym.source_path_ = path;
  sym.component_ = component;
  sym.size_info_ = info;
  info->raw_symbols.push_back(sym);
}

std::unique_ptr<SizeInfo> CreateSizeInfo() {
  std::unique_ptr<SizeInfo> info = std::make_unique<SizeInfo>();
  MakeSymbol(info.get(), SectionId::kText, 20, "a/b/c", "A");
  MakeSymbol(info.get(), SectionId::kText, 30, "a/b", "B");
  return info;
}

void CheckAllTreeNodesFindable(TreeBuilder& tree, const Json::Value& node) {
  const Json::Value& children = node["children"];
  for (unsigned int i = 0; i < children.size(); i++) {
    // Only recurse on folders, which have type Ct or Dt rather than t.
    if (children[i]["type"].size() == 2) {
      std::string id_path = children[i]["idPath"].asString();
      CheckAllTreeNodesFindable(tree, tree.Open(id_path.c_str()));
    }
  }
}

TEST(DiffTest, TestIdPathLens) {
  std::unique_ptr<SizeInfo> size_info = CreateSizeInfo();

  TreeBuilder builder(size_info.get());
  std::vector<std::function<bool(const BaseSymbol&)>> filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters);
  CheckAllTreeNodesFindable(builder, builder.Open(""));
  EXPECT_EQ(builder.Open("")["type"].asString(), "Dt");
}

TEST(DiffTest, TestComponentLens) {
  std::unique_ptr<SizeInfo> size_info = CreateSizeInfo();

  TreeBuilder builder(size_info.get());
  std::vector<std::function<bool(const BaseSymbol&)>> filters;
  builder.Build(std::make_unique<ComponentLens>(), '>', false, filters);
  CheckAllTreeNodesFindable(builder, builder.Open(""));
  EXPECT_EQ(builder.Open("A")["type"].asString(), "Ct");
  EXPECT_EQ(builder.Open("A")["size"].asInt(), 20);
  EXPECT_EQ(builder.Open("B")["type"].asString(), "Ct");
  EXPECT_EQ(builder.Open("B")["size"].asInt(), 30);
}

TEST(DiffTest, TestTemplateLens) {
  std::unique_ptr<SizeInfo> size_info = std::make_unique<SizeInfo>();
  MakeSymbol(size_info.get(), SectionId::kText, 20, "a/b/c", "A",
             "base::internal::Invoker<base::internal::BindState<void "
             "(autofill_assistant::Controller::*)(), "
             "base::WeakPtr<autofill_assistant::Controller> >, void "
             "()>::RunOnce(base::internal::BindStateBase*)");
  MakeSymbol(size_info.get(), SectionId::kText, 30, "a/b", "B",
             "base::internal::Invoker<base::internal::BindState<void "
             "(autofill_assistant::Controller::*)(int), "
             "base::WeakPtr<autofill_assistant::Controller>, unsigned int>, "
             "void ()>::RunOnce(base::internal::BindStateBase*)");

  for (Symbol& sym : size_info->raw_symbols) {
    sym.size_info_ = size_info.get();
  }

  TreeBuilder builder(size_info.get());
  std::vector<std::function<bool(const BaseSymbol&)>> filters;
  builder.Build(std::make_unique<TemplateLens>(), '/', false, filters);
  CheckAllTreeNodesFindable(builder, builder.Open(""));
  EXPECT_EQ(
      builder.Open("base::internal::Invoker<>::RunOnce")["type"].asString(),
      "Dt");
  EXPECT_EQ(builder.Open("base::internal::Invoker<>::RunOnce")["size"].asInt(),
            50);
}
}  // namespace
}  // namespace caspian
