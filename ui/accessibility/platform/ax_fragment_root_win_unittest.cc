// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/ax_platform_node_win_unittest.h"

#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>

#include "base/win/scoped_safearray.h"
#include "testing/gtest/include/gtest/gtest.h"

using Microsoft::WRL::ComPtr;

namespace ui {

class AXFragmentRootTest : public ui::AXPlatformNodeWinTest {};

TEST_F(AXFragmentRootTest, TestUIAGetFragmentRoot) {
  AXNodeData root;
  Init(root);
  InitFragmentRoot();

  ComPtr<IRawElementProviderFragmentRoot> expected_fragment_root =
      GetFragmentRoot();
  ComPtr<IRawElementProviderFragment> fragment_provider;
  expected_fragment_root.As(&fragment_provider);

  ComPtr<IRawElementProviderFragmentRoot> actual_fragment_root;
  EXPECT_HRESULT_SUCCEEDED(
      fragment_provider->get_FragmentRoot(&actual_fragment_root));
  EXPECT_EQ(expected_fragment_root.Get(), actual_fragment_root.Get());
}

TEST_F(AXFragmentRootTest, TestUIAGetRuntimeId) {
  AXNodeData root;
  Init(root);
  InitFragmentRoot();

  ComPtr<IRawElementProviderFragmentRoot> fragment_root_provider =
      GetFragmentRoot();
  ComPtr<IRawElementProviderFragment> fragment_provider;
  fragment_root_provider.As(&fragment_provider);

  base::win::ScopedSafearray runtime_id;
  EXPECT_HRESULT_SUCCEEDED(
      fragment_provider->GetRuntimeId(runtime_id.Receive()));
  EXPECT_EQ(runtime_id.Get(), nullptr);
}

TEST_F(AXFragmentRootTest, TestUIAElementProviderFromPoint) {
  AXNodeData root_data;
  root_data.id = 0;
  root_data.child_ids.push_back(1);
  root_data.child_ids.push_back(2);
  root_data.relative_bounds.bounds = gfx::RectF(0, 0, 80, 80);

  AXNodeData element1_data;
  element1_data.id = 1;
  element1_data.relative_bounds.bounds = gfx::RectF(0, 0, 50, 50);

  AXNodeData element2_data;
  element2_data.id = 2;
  element2_data.relative_bounds.bounds = gfx::RectF(0, 50, 30, 30);

  Init(root_data, element1_data, element2_data);
  InitFragmentRoot();

  AXNode* root_node = GetRootNode();
  AXNode* element1_node = root_node->children()[0];
  AXNode* element2_node = root_node->children()[1];

  ComPtr<IRawElementProviderFragmentRoot> fragment_root_prov(GetFragmentRoot());
  ComPtr<IRawElementProviderFragment> root_provider(
      GetRootIRawElementProviderFragment());
  ComPtr<IRawElementProviderFragment> element1_provider =
      QueryInterfaceFromNode<IRawElementProviderFragment>(element1_node);
  ComPtr<IRawElementProviderFragment> element2_provider =
      QueryInterfaceFromNode<IRawElementProviderFragment>(element2_node);

  ComPtr<IRawElementProviderFragment> provider_from_point;
  EXPECT_HRESULT_SUCCEEDED(fragment_root_prov->ElementProviderFromPoint(
      23, 31, &provider_from_point));
  EXPECT_EQ(element1_provider.Get(), provider_from_point.Get());

  EXPECT_HRESULT_SUCCEEDED(fragment_root_prov->ElementProviderFromPoint(
      23, 67, &provider_from_point));
  EXPECT_EQ(element2_provider.Get(), provider_from_point.Get());

  EXPECT_HRESULT_SUCCEEDED(fragment_root_prov->ElementProviderFromPoint(
      47, 67, &provider_from_point));
  EXPECT_EQ(root_provider.Get(), provider_from_point.Get());
}

TEST_F(AXFragmentRootTest, TestUIAGetFocus) {
  AXNodeData root_data;
  root_data.id = 0;
  root_data.child_ids.push_back(1);
  root_data.child_ids.push_back(2);

  AXNodeData element1_data;
  element1_data.id = 1;

  AXNodeData element2_data;
  element2_data.id = 2;

  Init(root_data, element1_data, element2_data);
  InitFragmentRoot();

  AXNode* root_node = GetRootNode();
  AXNode* element1_node = root_node->children()[0];
  AXNode* element2_node = root_node->children()[1];

  ComPtr<IRawElementProviderFragmentRoot> fragment_root_prov(GetFragmentRoot());
  ComPtr<IRawElementProviderFragment> root_provider(
      GetRootIRawElementProviderFragment());
  ComPtr<IRawElementProviderFragment> element1_provider =
      QueryInterfaceFromNode<IRawElementProviderFragment>(element1_node);
  ComPtr<IRawElementProviderFragment> element2_provider =
      QueryInterfaceFromNode<IRawElementProviderFragment>(element2_node);

  ComPtr<IRawElementProviderFragment> focused_fragment;
  EXPECT_HRESULT_SUCCEEDED(root_provider->SetFocus());
  EXPECT_HRESULT_SUCCEEDED(fragment_root_prov->GetFocus(&focused_fragment));
  EXPECT_EQ(root_provider.Get(), focused_fragment.Get());

  EXPECT_HRESULT_SUCCEEDED(element1_provider->SetFocus());
  EXPECT_HRESULT_SUCCEEDED(fragment_root_prov->GetFocus(&focused_fragment));
  EXPECT_EQ(element1_provider.Get(), focused_fragment.Get());

  EXPECT_HRESULT_SUCCEEDED(element2_provider->SetFocus());
  EXPECT_HRESULT_SUCCEEDED(fragment_root_prov->GetFocus(&focused_fragment));
  EXPECT_EQ(element2_provider.Get(), focused_fragment.Get());
}

TEST_F(AXFragmentRootTest, TestUIAErrorHandling) {
  AXNodeData root;
  Init(root);
  InitFragmentRoot();

  ComPtr<IRawElementProviderSimple> simple_provider =
      GetRootIRawElementProviderSimple();
  ComPtr<IRawElementProviderFragment> fragment_provider =
      GetRootIRawElementProviderFragment();
  ComPtr<IRawElementProviderFragmentRoot> fragment_root_provider =
      GetFragmentRoot();

  tree_.reset(new AXTree());
  ax_fragment_root_.reset(nullptr);

  ComPtr<IRawElementProviderSimple> returned_simple_provider;
  ComPtr<IRawElementProviderFragment> returned_fragment_provider;
  ComPtr<IRawElementProviderFragmentRoot> returned_fragment_root_provider;
  base::win::ScopedSafearray returned_runtime_id;

  EXPECT_EQ(
      static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
      simple_provider->get_HostRawElementProvider(&returned_simple_provider));

  EXPECT_EQ(
      static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
      fragment_provider->get_FragmentRoot(&returned_fragment_root_provider));

  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            fragment_provider->GetRuntimeId(returned_runtime_id.Receive()));

  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            fragment_root_provider->ElementProviderFromPoint(
                67, 23, &returned_fragment_provider));

  EXPECT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE),
            fragment_root_provider->GetFocus(&returned_fragment_provider));
}

TEST_F(AXFragmentRootTest, TestGetChildCount) {
  AXNodeData root;
  Init(root);
  InitFragmentRoot();

  AXPlatformNodeDelegate* fragment_root = ax_fragment_root_.get();
  EXPECT_EQ(1, fragment_root->GetChildCount());

  test_fragment_root_delegate_->child_ = nullptr;
  EXPECT_EQ(0, fragment_root->GetChildCount());
}

TEST_F(AXFragmentRootTest, TestChildAtIndex) {
  AXNodeData root;
  Init(root);
  InitFragmentRoot();

  gfx::NativeViewAccessible native_view_accessible =
      AXPlatformNodeFromNode(GetRootNode())->GetNativeViewAccessible();
  AXPlatformNodeDelegate* fragment_root = ax_fragment_root_.get();
  EXPECT_EQ(native_view_accessible, fragment_root->ChildAtIndex(0));
  EXPECT_EQ(nullptr, fragment_root->ChildAtIndex(1));

  test_fragment_root_delegate_->child_ = nullptr;
  EXPECT_EQ(nullptr, fragment_root->ChildAtIndex(0));
}

TEST_F(AXFragmentRootTest, TestGetParent) {
  AXNodeData root;
  Init(root);
  InitFragmentRoot();

  AXPlatformNodeDelegate* fragment_root = ax_fragment_root_.get();
  EXPECT_EQ(nullptr, fragment_root->GetParent());

  gfx::NativeViewAccessible native_view_accessible =
      AXPlatformNodeFromNode(GetRootNode())->GetNativeViewAccessible();
  test_fragment_root_delegate_->parent_ = native_view_accessible;
  EXPECT_EQ(native_view_accessible, fragment_root->GetParent());
}

}  // namespace ui
