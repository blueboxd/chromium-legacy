// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BrowserAccessibilityAndroidTest : public testing::Test {
 public:
  BrowserAccessibilityAndroidTest();
  ~BrowserAccessibilityAndroidTest() override;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  void SetUp() override;
  base::test::TaskEnvironment task_environment_;
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAndroidTest);
};

BrowserAccessibilityAndroidTest::BrowserAccessibilityAndroidTest() = default;

BrowserAccessibilityAndroidTest::~BrowserAccessibilityAndroidTest() = default;

void BrowserAccessibilityAndroidTest::SetUp() {
  ui::AXPlatformNode::NotifyAddAXModeFlags(ui::kAXModeComplete);
  test_browser_accessibility_delegate_ =
      std::make_unique<TestBrowserAccessibilityDelegate>();
}

TEST_F(BrowserAccessibilityAndroidTest, TestRetargetTextOnly) {
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName("Hello, world");

  ui::AXNodeData para1;
  para1.id = 11;
  para1.role = ax::mojom::Role::kParagraph;
  para1.child_ids = {text1.id};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {para1.id};

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, para1, text1),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* root_obj = manager->GetRoot();
  EXPECT_FALSE(root_obj->PlatformIsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());
  BrowserAccessibility* para_obj = root_obj->PlatformGetChild(0);
  EXPECT_TRUE(para_obj->PlatformIsLeaf());
  EXPECT_TRUE(para_obj->CanFireEvents());
  BrowserAccessibility* text_obj = manager->GetFromID(111);
  EXPECT_TRUE(text_obj->PlatformIsLeaf());
  EXPECT_FALSE(text_obj->CanFireEvents());
  BrowserAccessibility* updated = manager->RetargetForEvents(
      text_obj, BrowserAccessibilityManager::RetargetEventType::
                    RetargetEventTypeBlinkHover);
  // |updated| should be the paragraph.
  EXPECT_EQ(11, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());
  manager.reset();
}

TEST_F(BrowserAccessibilityAndroidTest, TestRetargetHeading) {
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData heading1;
  heading1.id = 11;
  heading1.role = ax::mojom::Role::kHeading;
  heading1.child_ids = {text1.id};
  heading1.SetName("heading");

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {heading1.id};

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, heading1, text1),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* root_obj = manager->GetRoot();
  EXPECT_FALSE(root_obj->PlatformIsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());
  BrowserAccessibility* heading_obj = root_obj->PlatformGetChild(0);
  EXPECT_TRUE(heading_obj->PlatformIsLeaf());
  EXPECT_TRUE(heading_obj->CanFireEvents());
  BrowserAccessibility* text_obj = manager->GetFromID(111);
  EXPECT_TRUE(text_obj->PlatformIsLeaf());
  EXPECT_FALSE(text_obj->CanFireEvents());
  BrowserAccessibility* updated = manager->RetargetForEvents(
      text_obj, BrowserAccessibilityManager::RetargetEventType::
                    RetargetEventTypeBlinkHover);
  // |updated| should be the heading.
  EXPECT_EQ(11, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());
  manager.reset();
}

TEST_F(BrowserAccessibilityAndroidTest, TestRetargetFocusable) {
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData para1;
  para1.id = 11;
  para1.role = ax::mojom::Role::kParagraph;
  para1.AddState(ax::mojom::State::kFocusable);
  para1.SetName("focusable");
  para1.child_ids = {text1.id};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {para1.id};

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, para1, text1),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* root_obj = manager->GetRoot();
  EXPECT_FALSE(root_obj->PlatformIsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());
  BrowserAccessibility* para_obj = root_obj->PlatformGetChild(0);
  EXPECT_TRUE(para_obj->PlatformIsLeaf());
  EXPECT_TRUE(para_obj->CanFireEvents());
  BrowserAccessibility* text_obj = manager->GetFromID(111);
  EXPECT_TRUE(text_obj->PlatformIsLeaf());
  EXPECT_FALSE(text_obj->CanFireEvents());
  BrowserAccessibility* updated = manager->RetargetForEvents(
      text_obj, BrowserAccessibilityManager::RetargetEventType::
                    RetargetEventTypeBlinkHover);
  // |updated| should be the paragraph.
  EXPECT_EQ(11, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());
  manager.reset();
}

TEST_F(BrowserAccessibilityAndroidTest, TestRetargetInputControl) {
  // Build the tree that has a form with input time.
  // +rootWebArea
  // ++genericContainer
  // +++form
  // ++++labelText
  // +++++staticText
  // ++++inputTime
  // +++++genericContainer
  // ++++++staticText
  // ++++button
  // +++++staticText
  ui::AXNodeData label_text;
  label_text.id = 11111;
  label_text.role = ax::mojom::Role::kStaticText;
  label_text.SetName("label");

  ui::AXNodeData label;
  label.id = 1111;
  label.role = ax::mojom::Role::kLabelText;
  label.child_ids = {label_text.id};

  ui::AXNodeData input_text;
  input_text.id = 111211;
  input_text.role = ax::mojom::Role::kStaticText;
  input_text.SetName("input_text");

  ui::AXNodeData input_container;
  input_container.id = 11121;
  input_container.role = ax::mojom::Role::kGenericContainer;
  input_container.child_ids = {input_text.id};

  ui::AXNodeData input_time;
  input_time.id = 1112;
  input_time.role = ax::mojom::Role::kInputTime;
  input_time.AddState(ax::mojom::State::kFocusable);
  input_time.child_ids = {input_container.id};

  ui::AXNodeData button_text;
  button_text.id = 11131;
  button_text.role = ax::mojom::Role::kStaticText;
  button_text.AddState(ax::mojom::State::kFocusable);
  button_text.SetName("button");

  ui::AXNodeData button;
  button.id = 1113;
  button.role = ax::mojom::Role::kButton;
  button.child_ids = {button_text.id};

  ui::AXNodeData form;
  form.id = 111;
  form.role = ax::mojom::Role::kForm;
  form.child_ids = {label.id, input_time.id, button.id};

  ui::AXNodeData container;
  container.id = 11;
  container.role = ax::mojom::Role::kGenericContainer;
  container.child_ids = {form.id};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {container.id};

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, container, form, label, label_text, input_time,
                           input_container, input_text, button, button_text),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* root_obj = manager->GetRoot();
  EXPECT_FALSE(root_obj->PlatformIsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());
  BrowserAccessibility* label_obj = manager->GetFromID(1111);
  EXPECT_TRUE(label_obj->PlatformIsLeaf());
  EXPECT_TRUE(label_obj->CanFireEvents());
  BrowserAccessibility* label_text_obj = manager->GetFromID(11111);
  EXPECT_TRUE(label_text_obj->PlatformIsLeaf());
  EXPECT_FALSE(label_text_obj->CanFireEvents());
  BrowserAccessibility* updated = manager->RetargetForEvents(
      label_text_obj, BrowserAccessibilityManager::RetargetEventType::
                          RetargetEventTypeBlinkHover);
  // |updated| should be the labelText.
  EXPECT_EQ(1111, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());

  BrowserAccessibility* input_time_obj = manager->GetFromID(1112);
  EXPECT_TRUE(input_time_obj->PlatformIsLeaf());
  EXPECT_TRUE(input_time_obj->CanFireEvents());
  BrowserAccessibility* input_time_container_obj = manager->GetFromID(11121);
  EXPECT_TRUE(input_time_container_obj->PlatformIsLeaf());
  EXPECT_FALSE(input_time_container_obj->CanFireEvents());
  updated = manager->RetargetForEvents(
      input_time_container_obj, BrowserAccessibilityManager::RetargetEventType::
                                    RetargetEventTypeBlinkHover);
  // |updated| should be the inputTime.
  EXPECT_EQ(1112, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());
  BrowserAccessibility* input_text_obj = manager->GetFromID(111211);
  EXPECT_TRUE(input_text_obj->PlatformIsLeaf());
  EXPECT_FALSE(input_text_obj->CanFireEvents());
  updated = manager->RetargetForEvents(
      input_text_obj, BrowserAccessibilityManager::RetargetEventType::
                          RetargetEventTypeBlinkHover);
  // |updated| should be the inputTime.
  EXPECT_EQ(1112, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());

  BrowserAccessibility* button_obj = manager->GetFromID(1113);
  EXPECT_TRUE(button_obj->PlatformIsLeaf());
  EXPECT_TRUE(button_obj->CanFireEvents());
  BrowserAccessibility* button_text_obj = manager->GetFromID(11131);
  EXPECT_TRUE(button_text_obj->PlatformIsLeaf());
  EXPECT_FALSE(button_text_obj->CanFireEvents());
  updated = manager->RetargetForEvents(
      button_text_obj, BrowserAccessibilityManager::RetargetEventType::
                           RetargetEventTypeBlinkHover);
  // |updated| should be the button.
  EXPECT_EQ(1113, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());
  manager.reset();
}

}  // namespace content
