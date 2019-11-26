// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate.h"

#include <atk/atk.h>

#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace test {

class ViewAXPlatformNodeDelegateAuraLinuxTest : public ViewsTestBase {
 public:
  ViewAXPlatformNodeDelegateAuraLinuxTest() = default;
  ~ViewAXPlatformNodeDelegateAuraLinuxTest() override = default;
};

TEST_F(ViewAXPlatformNodeDelegateAuraLinuxTest, TextfieldAccessibility) {
  Widget widget;
  Widget::InitParams init_params = CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));

  View* content = new View;
  widget.SetContentsView(content);

  Textfield* textfield = new Textfield;
  textfield->SetAccessibleName(base::UTF8ToUTF16("Name"));
  textfield->SetText(base::UTF8ToUTF16("Value"));
  content->AddChildView(textfield);

  AtkText* atk_text = ATK_TEXT(textfield->GetNativeViewAccessible());
  ASSERT_NE(nullptr, atk_text);

  struct TextChangeData {
    int position;
    int length;
    std::string text;
  };

  std::vector<TextChangeData> text_remove_events;
  std::vector<TextChangeData> text_insert_events;
  GCallback callback = G_CALLBACK(
      +[](AtkText*, int position, int length, char* text, gpointer data) {
        auto* events = static_cast<std::vector<TextChangeData>*>(data);
        events->push_back(TextChangeData{position, length, text});
      });
  g_signal_connect(atk_text, "text-insert", callback, &text_insert_events);
  g_signal_connect(atk_text, "text-remove", callback, &text_remove_events);

  textfield->NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  ASSERT_EQ(text_remove_events.size(), 0ul);
  ASSERT_EQ(text_insert_events.size(), 1ul);
  ASSERT_EQ(text_insert_events[0].position, 0);
  ASSERT_EQ(text_insert_events[0].length, 5);
  ASSERT_EQ(text_insert_events[0].text, "Value");
  text_insert_events.clear();

  textfield->SetText(base::UTF8ToUTF16("Value A"));
  textfield->NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);

  ASSERT_EQ(text_remove_events.size(), 0ul);
  ASSERT_EQ(text_insert_events.size(), 1ul);
  ASSERT_EQ(text_insert_events[0].position, 5);
  ASSERT_EQ(text_insert_events[0].length, 2);
  ASSERT_EQ(text_insert_events[0].text, " A");
  text_insert_events.clear();

  textfield->SetText(base::UTF8ToUTF16("Value"));
  textfield->NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  ASSERT_EQ(text_remove_events.size(), 1ul);
  ASSERT_EQ(text_insert_events.size(), 0ul);
  ASSERT_EQ(text_remove_events[0].position, 5);
  ASSERT_EQ(text_remove_events[0].length, 2);
  ASSERT_EQ(text_remove_events[0].text, " A");
  text_remove_events.clear();

  textfield->SetText(base::UTF8ToUTF16("Prefix Value"));
  textfield->NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  ASSERT_EQ(text_remove_events.size(), 0ul);
  ASSERT_EQ(text_insert_events.size(), 1ul);
  ASSERT_EQ(text_insert_events[0].position, 0);
  ASSERT_EQ(text_insert_events[0].length, 7);
  ASSERT_EQ(text_insert_events[0].text, "Prefix ");
  text_insert_events.clear();

  textfield->SetText(base::UTF8ToUTF16("Value"));
  textfield->NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  ASSERT_EQ(text_remove_events.size(), 1ul);
  ASSERT_EQ(text_insert_events.size(), 0ul);
  ASSERT_EQ(text_remove_events[0].position, 0);
  ASSERT_EQ(text_remove_events[0].length, 7);
  ASSERT_EQ(text_remove_events[0].text, "Prefix ");
  text_insert_events.clear();
}

TEST_F(ViewAXPlatformNodeDelegateAuraLinuxTest, AuraChildWidgets) {
  // Create the parent widget.
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = gfx::Rect(0, 0, 400, 200);
  widget.Init(std::move(init_params));
  widget.Show();

  // Initially it has 1 child.
  AtkObject* root_view_accessible =
      widget.GetRootView()->GetNativeViewAccessible();
  ASSERT_EQ(1, atk_object_get_n_accessible_children(root_view_accessible));

  // Create the child widget, one of two ways (see below).
  Widget child_widget;
  Widget::InitParams child_init_params =
      CreateParams(Widget::InitParams::TYPE_BUBBLE);
  child_init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_init_params.parent = widget.GetNativeView();
  child_init_params.bounds = gfx::Rect(30, 40, 100, 50);
  child_init_params.child = false;
  child_widget.Init(std::move(child_init_params));
  child_widget.Show();

  // Now the AtkObject for the parent widget should have 2 children.
  ASSERT_EQ(2, atk_object_get_n_accessible_children(root_view_accessible));

  // Make sure that querying the parent of the child gets us back to
  // the original parent.
  AtkObject* child_widget_accessible =
      child_widget.GetRootView()->GetNativeViewAccessible();
  ASSERT_EQ(atk_object_get_parent(child_widget_accessible),
            root_view_accessible);

  // Make sure that querying the second child of the parent is the child widget
  // accessible as well.
  AtkObject* second_child =
      atk_object_ref_accessible_child(root_view_accessible, 1);
  ASSERT_EQ(second_child, child_widget_accessible);
  g_object_unref(second_child);
}

}  // namespace test
}  // namespace views
