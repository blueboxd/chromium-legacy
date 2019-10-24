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
#include "base/no_destructor.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/color_picker_view.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP = 13;
constexpr int TAB_GROUP_HEADER_CXMENU_UNGROUP = 14;
constexpr int TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP = 15;

// Returns our hard-coded set of colors.
const std::vector<std::pair<SkColor, base::string16>>& GetColorPickerList() {
  static const base::NoDestructor<
      std::vector<std::pair<SkColor, base::string16>>>
      list({{gfx::kGoogleBlue600, base::ASCIIToUTF16("Blue")},
            {gfx::kGoogleRed600, base::ASCIIToUTF16("Red")},
            {gfx::kGoogleYellow600, base::ASCIIToUTF16("Yellow")},
            {gfx::kGoogleGreen600, base::ASCIIToUTF16("Green")},
            {gfx::kGoogleOrange600, base::ASCIIToUTF16("Orange")},
            {gfx::kGooglePink600, base::ASCIIToUTF16("Pink")},
            {gfx::kGooglePurple600, base::ASCIIToUTF16("Purple")},
            {gfx::kGoogleCyan600, base::ASCIIToUTF16("Cyan")}});
  return *list;
}
}  // namespace

// static
views::Widget* TabGroupEditorBubbleView::Show(TabGroupHeader* anchor_view,
                                              TabController* tab_controller,
                                              TabGroupId group) {
  views::Widget* const widget = BubbleDialogDelegateView::CreateBubble(
      new TabGroupEditorBubbleView(anchor_view, tab_controller, group));
  widget->Show();
  return widget;
}

gfx::Size TabGroupEditorBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

base::string16 TabGroupEditorBubbleView::GetWindowTitle() const {
  // TODO(crbug.com/989174): remove this and other hardcoded ASCII strings.
  return base::ASCIIToUTF16("Customize tab group visuals");
}

ui::ModalType TabGroupEditorBubbleView::GetModalType() const {
  return ui::MODAL_TYPE_NONE;
}

int TabGroupEditorBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

TabGroupEditorBubbleView::TabGroupEditorBubbleView(
    TabGroupHeader* anchor_view,
    TabController* tab_controller,
    TabGroupId group)
    : tab_controller_(tab_controller),
      group_(group),
      title_field_controller_(this),
      menu_button_listener_(tab_controller, anchor_view, group) {
  SetAnchorView(anchor_view);

  // Title insets assume there is content (and thus have no bottom padding). Use
  // dialog insets to get the bottom margin back.
  set_title_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  set_margins(gfx::Insets());

  const auto* layout_provider = ChromeLayoutProvider::Get();
  const TabGroupVisualData* current_data =
      tab_controller_->GetVisualDataForGroup(group_);

  // Layout vertically with margin collapsing. This allows us to use spacer
  // views with |DISTANCE_UNRELATED_CONTROL_VERTICAL| margins without worrying
  // about the default |DISTANCE_RELATED_CONTROL_VERTICAL| spacing.
  auto container_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL),
      true /* collapse_margins_spacing */);
  container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  views::View* group_modifier_container =
      AddChildView(std::make_unique<views::View>());
  group_modifier_container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL))));
  group_modifier_container->SetLayoutManager(std::move(container_layout));

  // Add the text field for editing the title.
  title_field_ = group_modifier_container->AddChildView(
      std::make_unique<views::Textfield>());
  title_field_->SetDefaultWidthInChars(15);
  title_field_->SetText(current_data->title());
  title_field_->SetAccessibleName(base::ASCIIToUTF16("Group title"));
  title_field_->set_controller(&title_field_controller_);

  title_field_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(0, 0,
                  layout_provider->GetDistanceMetric(
                      views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                  0));

  color_selector_ =
      group_modifier_container->AddChildView(std::make_unique<ColorPickerView>(
          GetColorPickerList(),
          base::Bind(&TabGroupEditorBubbleView::UpdateGroup,
                     base::Unretained(this))));

  AddChildView(std::make_unique<views::Separator>());

  std::unique_ptr<views::LabelButton> new_tab_menu_item = CreateBubbleMenuItem(
      TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP),
      &menu_button_listener_);
  AddChildView(std::move(new_tab_menu_item));

  std::unique_ptr<views::LabelButton> ungroup_menu_item = CreateBubbleMenuItem(
      TAB_GROUP_HEADER_CXMENU_UNGROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_UNGROUP),
      &menu_button_listener_);
  AddChildView(std::move(ungroup_menu_item));

  std::unique_ptr<views::LabelButton> close_menu_item = CreateBubbleMenuItem(
      TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP),
      &menu_button_listener_);
  AddChildView(std::move(close_menu_item));

  views::FlexLayout* layout_manager_ =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kVertical)
      .SetIgnoreDefaultMainAxisMargins(true);
}

TabGroupEditorBubbleView::~TabGroupEditorBubbleView() = default;

void TabGroupEditorBubbleView::UpdateGroup() {
  TabGroupVisualData old_data = *tab_controller_->GetVisualDataForGroup(group_);

  base::Optional<SkColor> selected_color = color_selector_->GetSelectedColor();
  const SkColor color =
      selected_color.has_value() ? selected_color.value() : old_data.color();
  TabGroupVisualData new_data(title_field_->GetText(), color);

  tab_controller_->SetVisualDataForGroup(group_, new_data);
}

void TabGroupEditorBubbleView::TitleFieldController::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
  DCHECK_EQ(sender, parent_->title_field_);
  parent_->UpdateGroup();
}

TabGroupEditorBubbleView::ButtonListener::ButtonListener(
    TabController* tab_controller,
    TabGroupHeader* anchor_view,
    TabGroupId group)
    : tab_controller_(tab_controller),
      anchor_view_(anchor_view),
      group_(group) {}

void TabGroupEditorBubbleView::ButtonListener::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  switch (sender->GetID()) {
    case TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP:
      tab_controller_->AddNewTabInGroup(group_);

      break;
    case TAB_GROUP_HEADER_CXMENU_UNGROUP:
      anchor_view_->RemoveObserverFromWidget(sender->GetWidget());
      tab_controller_->UngroupAllTabsInGroup(group_);
      break;
    case TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP:
      tab_controller_->CloseAllTabsInGroup(group_);
      break;
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
