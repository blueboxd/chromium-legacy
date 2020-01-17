// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/color_picker_view.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

// static
views::Widget* TabGroupEditorBubbleView::Show(
    TabGroupHeader* anchor_view,
    TabController* tab_controller,
    const tab_groups::TabGroupId& group) {
  views::Widget* const widget = BubbleDialogDelegateView::CreateBubble(
      new TabGroupEditorBubbleView(anchor_view, tab_controller, group));
  widget->Show();
  return widget;
}

gfx::Size TabGroupEditorBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_TABSTRIP_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType TabGroupEditorBubbleView::GetModalType() const {
  return ui::MODAL_TYPE_NONE;
}

views::View* TabGroupEditorBubbleView::GetInitiallyFocusedView() {
  return title_field_;
}

TabGroupEditorBubbleView::TabGroupEditorBubbleView(
    TabGroupHeader* anchor_view,
    TabController* tab_controller,
    const tab_groups::TabGroupId& group)
    : tab_controller_(tab_controller),
      group_(group),
      title_field_controller_(this),
      button_listener_(tab_controller, anchor_view, group) {
  SetAnchorView(anchor_view);
  set_margins(gfx::Insets());

  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);

  const auto* layout_provider = ChromeLayoutProvider::Get();
  const base::string16 title = tab_controller_->GetGroupTitle(group_);
  const int horizontal_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_menu_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  // The vertical spacing for the non menu items within the editor bubble.
  const int vertical_dialog_content_spacing = 16;

  views::View* group_modifier_container =
      AddChildView(std::make_unique<views::View>());

  gfx::Insets color_element_insets =
      ChromeLayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_VECTOR_IMAGE_BUTTON);
  group_modifier_container->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_dialog_content_spacing,
                  horizontal_spacing - color_element_insets.left(),
                  vertical_dialog_content_spacing,
                  horizontal_spacing - color_element_insets.right())));

  views::FlexLayout* container_layout =
      group_modifier_container->SetLayoutManager(
          std::make_unique<views::FlexLayout>());
  container_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetIgnoreDefaultMainAxisMargins(true);

  // Add the text field for editing the title.
  views::View* title_field_container =
      group_modifier_container->AddChildView(std::make_unique<views::View>());
  title_field_container->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      0, color_element_insets.left(), vertical_dialog_content_spacing,
      color_element_insets.right())));
  title_field_ =
      title_field_container->AddChildView(std::make_unique<views::Textfield>());
  title_field_->SetText(title);
  title_field_->SetAccessibleName(base::ASCIIToUTF16("Group title"));
  title_field_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_BUBBLE_TITLE_PLACEHOLDER));
  title_field_->set_controller(&title_field_controller_);

  views::FlexLayout* title_field_container_layout =
      title_field_container->SetLayoutManager(
          std::make_unique<views::FlexLayout>());
  title_field_container_layout
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetIgnoreDefaultMainAxisMargins(true);

  const SkColor initial_color = InitColorSet();
  color_selector_ =
      group_modifier_container->AddChildView(std::make_unique<ColorPickerView>(
          colors_, background_color(), initial_color,
          base::Bind(&TabGroupEditorBubbleView::UpdateGroup,
                     base::Unretained(this))));

  AddChildView(std::make_unique<views::Separator>());

  views::View* menu_items_container =
      AddChildView(std::make_unique<views::View>());
  menu_items_container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(vertical_menu_spacing, 0)));
  views::FlexLayout* layout_manager_ = menu_items_container->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kVertical)
      .SetIgnoreDefaultMainAxisMargins(true);

  gfx::Insets menu_item_border_inset =
      gfx::Insets(vertical_menu_spacing, horizontal_spacing);

  std::unique_ptr<views::LabelButton> new_tab_menu_item = CreateBubbleMenuItem(
      TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP),
      &button_listener_);
  new_tab_menu_item->SetBorder(
      views::CreateEmptyBorder(menu_item_border_inset));
  menu_items_container->AddChildView(std::move(new_tab_menu_item));

  std::unique_ptr<views::LabelButton> ungroup_menu_item = CreateBubbleMenuItem(
      TAB_GROUP_HEADER_CXMENU_UNGROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_UNGROUP),
      &button_listener_);
  ungroup_menu_item->SetBorder(
      views::CreateEmptyBorder(menu_item_border_inset));
  menu_items_container->AddChildView(std::move(ungroup_menu_item));

  std::unique_ptr<views::LabelButton> close_menu_item = CreateBubbleMenuItem(
      TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP),
      &button_listener_);
  close_menu_item->SetBorder(views::CreateEmptyBorder(menu_item_border_inset));
  menu_items_container->AddChildView(std::move(close_menu_item));

  std::unique_ptr<views::LabelButton> feedback_menu_item = CreateBubbleMenuItem(
      TAB_GROUP_HEADER_CXMENU_FEEDBACK,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_SEND_FEEDBACK),
      &button_listener_);
  feedback_menu_item->SetBorder(
      views::CreateEmptyBorder(menu_item_border_inset));
  menu_items_container->AddChildView(std::move(feedback_menu_item));

  views::FlexLayout* menu_layout_manager_ =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  menu_layout_manager_->SetOrientation(views::LayoutOrientation::kVertical);
}

TabGroupEditorBubbleView::~TabGroupEditorBubbleView() = default;

SkColor TabGroupEditorBubbleView::InitColorSet() {
  base::flat_map<tab_groups::TabGroupColorId, tab_groups::TabGroupColor>
      all_colors = tab_groups::GetTabGroupColorSet();
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  // Keep track of the current group's color, to be returned as the initial
  // selected value.
  const tab_groups::TabGroupColorId initial_color_id =
      tab_controller_->GetGroupColorId(group_);
  SkColor initial_color;

  color_ids_.reserve(all_colors.size());
  colors_.reserve(all_colors.size());
  for (auto const color_pair : all_colors) {
    color_ids_.push_back(color_pair.first);
    SkColor color = native_theme->ShouldUseDarkColors()
                        ? color_pair.second.dark_theme_color
                        : color_pair.second.light_theme_color;
    colors_.push_back({color, color_pair.second.label});

    if (color_pair.first == initial_color_id)
      initial_color = color;
  }

  return initial_color;
}

void TabGroupEditorBubbleView::UpdateGroup() {
  base::Optional<int> selected_element = color_selector_->GetSelectedElement();
  const tab_groups::TabGroupColorId color_id =
      selected_element.has_value() ? color_ids_[selected_element.value()]
                                   : tab_controller_->GetGroupColorId(group_);
  tab_groups::TabGroupVisualData new_data(title_field_->GetText(), color_id);
  tab_controller_->SetVisualDataForGroup(group_, new_data);
}

void TabGroupEditorBubbleView::TitleFieldController::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
  DCHECK_EQ(sender, parent_->title_field_);
  parent_->UpdateGroup();
}

bool TabGroupEditorBubbleView::TitleFieldController::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& key_event) {
  DCHECK_EQ(sender, parent_->title_field_);

  const ui::KeyboardCode key_code = key_event.key_code();
  if (key_code == ui::VKEY_RETURN || key_code == ui::VKEY_ESCAPE) {
    parent_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    return true;
  }

  return false;
}

TabGroupEditorBubbleView::ButtonListener::ButtonListener(
    TabController* tab_controller,
    TabGroupHeader* anchor_view,
    tab_groups::TabGroupId group)
    : tab_controller_(tab_controller),
      anchor_view_(anchor_view),
      group_(group) {}

void TabGroupEditorBubbleView::ButtonListener::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  switch (sender->GetID()) {
    case TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP:
      base::RecordAction(
          base::UserMetricsAction("TabGroups_TabGroupBubble_NewTabInGroup"));
      tab_controller_->AddNewTabInGroup(group_);
      break;
    case TAB_GROUP_HEADER_CXMENU_UNGROUP:
      anchor_view_->RemoveObserverFromWidget(sender->GetWidget());
      tab_controller_->UngroupAllTabsInGroup(group_);
      break;
    case TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP:
      tab_controller_->CloseAllTabsInGroup(group_);
      break;
    case TAB_GROUP_HEADER_CXMENU_FEEDBACK: {
      const Browser* browser = tab_controller_->GetBrowser();
      chrome::ShowFeedbackPage(
          browser, chrome::FeedbackSource::kFeedbackSourceDesktopTabGroups,
          std::string() /* description_template */,
          std::string() /* description_placeholder_text */,
          std::string("DESKTOP_TAB_GROUPS") /* category_tag */,
          std::string() /* extra_diagnostics */);
      break;
    }
    default:
      NOTREACHED();
  }

  // In the case of closing the tabs in a group or ungrouping the tabs, the
  // widget should be closed because it is no longer applicable. In the case of
  // opening a new tab in the group, the widget is closed to allow users to
  // continue their work in their newly created tab.
  sender->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
}
