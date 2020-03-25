// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"

namespace ui {
namespace {

void MakeStaticText(AXNodeData* node, int id, const std::string& text) {
  node->id = id;
  node->role = ax::mojom::Role::kStaticText;
  node->SetName(text);
}

void MakeGroup(AXNodeData* node, int id, std::vector<int> child_ids) {
  node->id = id;
  node->role = ax::mojom::Role::kGroup;
  node->child_ids = child_ids;
}

void SetIsInvisible(AXTree* tree, int id, bool invisible) {
  AXTreeUpdate update;
  update.nodes.resize(1);
  update.nodes[0] = tree->GetFromId(id)->data();
  if (invisible)
    update.nodes[0].AddState(ax::mojom::State::kInvisible);
  else
    update.nodes[0].RemoveState(ax::mojom::State::kInvisible);
  tree->Unserialize(update);
}

void SetRole(AXTree* tree, int id, ax::mojom::Role role) {
  AXTreeUpdate update;
  update.nodes.resize(1);
  update.nodes[0] = tree->GetFromId(id)->data();
  update.nodes[0].role = role;
  tree->Unserialize(update);
}

}  // namespace

TEST(AXPlatformNodeBaseTest, InnerTextIgnoresInvisibleAndIgnored) {
  AXTreeUpdate update;

  update.root_id = 1;
  update.nodes.resize(6);

  MakeStaticText(&update.nodes[1], 2, "a");
  MakeStaticText(&update.nodes[2], 3, "b");

  MakeStaticText(&update.nodes[4], 5, "d");
  MakeStaticText(&update.nodes[5], 6, "e");

  MakeGroup(&update.nodes[3], 4, {5, 6});
  MakeGroup(&update.nodes[0], 1, {2, 3, 4});

  AXTree tree(update);

  auto* root = static_cast<AXPlatformNodeBase*>(
      TestAXNodeWrapper::GetOrCreate(&tree, tree.root())->ax_platform_node());

  // Set an AXMode on the AXPlatformNode as some platforms (auralinux) use it to
  // determine if it should enable accessibility.
  AXPlatformNodeBase::NotifyAddAXModeFlags(kAXModeComplete);

  EXPECT_EQ(root->GetInnerText(), base::UTF8ToUTF16("abde"));

  // Setting invisible or ignored on a static text node causes it to be included
  // or excluded from the root node's inner text:
  {
    SetIsInvisible(&tree, 2, true);
    EXPECT_EQ(root->GetInnerText(), base::UTF8ToUTF16("bde"));

    SetIsInvisible(&tree, 2, false);
    EXPECT_EQ(root->GetInnerText(), base::UTF8ToUTF16("abde"));

    SetRole(&tree, 2, ax::mojom::Role::kIgnored);
    EXPECT_EQ(root->GetInnerText(), base::UTF8ToUTF16("bde"));

    SetRole(&tree, 2, ax::mojom::Role::kStaticText);
    EXPECT_EQ(root->GetInnerText(), base::UTF8ToUTF16("abde"));
  }

  // Setting invisible or ignored on a group node has no effect on the inner
  // text:
  {
    SetIsInvisible(&tree, 4, true);
    EXPECT_EQ(root->GetInnerText(), base::UTF8ToUTF16("abde"));

    SetRole(&tree, 4, ax::mojom::Role::kIgnored);
    EXPECT_EQ(root->GetInnerText(), base::UTF8ToUTF16("abde"));
  }
}

}  // namespace ui
