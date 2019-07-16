// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hover_button.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::Border> CreateBorderWithVerticalSpacing(
    int vert_spacing) {
  const int horz_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_HORIZONTAL_PADDING);
  return views::CreateEmptyBorder(vert_spacing, horz_spacing, vert_spacing,
                                  horz_spacing);
}

// Sets the accessible name of |parent| to the text from |first| and |second|.
// Also set the combined text as the tooltip for |parent| if |set_tooltip| is
// true and either |first| or |second|'s text is cut off or elided.
void SetTooltipAndAccessibleName(views::Button* parent,
                                 views::StyledLabel* first,
                                 views::Label* second,
                                 const gfx::Rect& available_space,
                                 int taken_width,
                                 bool set_tooltip) {
  const base::string16 accessible_name =
      second == nullptr
          ? first->GetText()
          : base::JoinString({first->GetText(), second->GetText()},
                             base::ASCIIToUTF16("\n"));
  if (set_tooltip) {
    const int available_width = available_space.width() - taken_width;

    // |views::StyledLabel|s only add tooltips for any links they may have.
    // However, since |HoverButton| will never insert a link inside its child
    // |StyledLabel|, decide whether it needs a tooltip by checking whether the
    // available space is smaller than its preferred size.
    bool first_truncated = first->GetPreferredSize().width() > available_width;
    bool second_truncated = false;
    if (second != nullptr)
      second_truncated = second->GetPreferredSize().width() > available_width;

    parent->SetTooltipText(first_truncated || second_truncated
                               ? accessible_name
                               : base::string16());
  }
  parent->SetAccessibleName(accessible_name);
}

}  // namespace

HoverButton::HoverButton(views::ButtonListener* button_listener,
                         const base::string16& text)
    : views::MenuButton(text, this), listener_(button_listener) {
  SetInstallFocusRingOnFocus(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  const int vert_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTROL_LIST_VERTICAL);
  SetBorder(CreateBorderWithVerticalSpacing(vert_spacing));

  SetInkDropMode(InkDropMode::ON);

  set_notify_action(Button::NOTIFY_ON_RELEASE);
}

HoverButton::HoverButton(views::ButtonListener* button_listener,
                         const gfx::ImageSkia& icon,
                         const base::string16& text)
    : HoverButton(button_listener, text) {
  SetImage(STATE_NORMAL, icon);
}

HoverButton::HoverButton(views::ButtonListener* button_listener,
                         std::unique_ptr<views::View> icon_view,
                         const base::string16& title,
                         const base::string16& subtitle,
                         std::unique_ptr<views::View> secondary_view,
                         bool resize_row_for_secondary_view,
                         bool secondary_view_can_process_events)
    : HoverButton(button_listener, base::string16()) {
  label()->SetHandlesTooltips(false);
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // The vertical spacing above and below the icon. If the icon is small, use
  // more vertical spacing.
  constexpr int kLargeIconHeight = 20;
  const int icon_height = icon_view->GetPreferredSize().height();
  const bool is_small_icon = icon_height <= kLargeIconHeight;
  int remaining_vert_spacing =
      is_small_icon
          ? layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL)
          : 12;
  const int total_height = icon_height + remaining_vert_spacing * 2;

  // If the padding given to the top and bottom of the HoverButton (i.e., on
  // either side of the |icon_view|) overlaps with the combined line height of
  // the |title_| and |subtitle_|, calculate the remaining padding that is
  // required to maintain a constant amount of padding above and below the icon.
  const int num_labels = subtitle.empty() ? 1 : 2;
  const int combined_line_height =
      views::style::GetLineHeight(views::style::CONTEXT_LABEL,
                                  STYLE_SECONDARY) *
      num_labels;
  if (combined_line_height > icon_height)
    remaining_vert_spacing = (total_height - combined_line_height) / 2;

  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  // Badging may make the icon slightly wider (but not taller). However, the
  // layout should be the same whether or not the icon is badged, so allow the
  // badged part of the icon to extend into the padding.
  const int badge_spacing = icon_view->GetPreferredSize().width() - icon_height;
  const int icon_label_spacing = layout_provider->GetDistanceMetric(
                                     views::DISTANCE_RELATED_LABEL_HORIZONTAL) -
                                 badge_spacing;

  constexpr int kColumnSetId = 0;
  views::ColumnSet* columns = grid_layout->AddColumnSet(kColumnSetId);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddPaddingColumn(views::GridLayout::kFixedSize, icon_label_spacing);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                     views::GridLayout::USE_PREF, 0, 0);

  taken_width_ = GetInsets().width() + icon_view->GetPreferredSize().width() +
                 icon_label_spacing;

  // Make sure hovering over the icon also hovers the |HoverButton|.
  icon_view->set_can_process_events_within_subtree(false);
  // Don't cover |icon_view| when the ink drops are being painted. |MenuButton|
  // already does this with its own image.
  icon_view->SetPaintToLayer();
  icon_view->layer()->SetFillsBoundsOpaquely(false);
  // Split the two rows evenly between the total height minus the padding.
  const int row_height =
      (total_height - remaining_vert_spacing * 2) / num_labels;
  grid_layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId,
                        row_height);
  icon_view_ = grid_layout->AddView(std::move(icon_view), 1, num_labels);

  auto title_label = std::make_unique<views::StyledLabel>(title, nullptr);
  // Size without a maximum width to get a single line label.
  title_label->SizeToFit(0);
  // |views::StyledLabel|s are all multi-line. With a layout manager,
  // |StyledLabel| will try use the available space to size itself, and long
  // titles will wrap to the next line (for smaller |HoverButton|s, this will
  // also cover up |subtitle_|). Wrap it in a parent view with no layout manager
  // to ensure it keeps its original size set by SizeToFit() above. Long titles
  // will then be truncated.
  auto title_wrapper = std::make_unique<views::View>();
  title_ = title_wrapper->AddChildView(std::move(title_label));
  // Hover the whole button when hovering |title_|. This is OK because |title_|
  // will never have a link in it.
  title_wrapper->set_can_process_events_within_subtree(false);
  grid_layout->AddView(std::move(title_wrapper));

  if (secondary_view) {
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::USE_PREF, 0, 0);
    secondary_view->set_can_process_events_within_subtree(
        secondary_view_can_process_events);
    // |secondary_view| needs a layer otherwise it's obscured by the layer
    // used in drawing ink drops.
    secondary_view->SetPaintToLayer();
    secondary_view->layer()->SetFillsBoundsOpaquely(false);
    secondary_view_ =
        grid_layout->AddView(std::move(secondary_view), 1, num_labels);

    if (!resize_row_for_secondary_view) {
      insets_ = views::MenuButton::GetInsets();
      auto secondary_ctl_size = secondary_view_->GetPreferredSize();
      if (secondary_ctl_size.height() > row_height) {
        // Secondary view is larger. Reduce the insets.
        int reduced_inset = (secondary_ctl_size.height() - row_height) / 2;
        insets_.value().set_top(
            std::max(insets_.value().top() - reduced_inset, 0));
        insets_.value().set_bottom(
            std::max(insets_.value().bottom() - reduced_inset, 0));
      }
    }
  }

  if (!subtitle.empty()) {
    grid_layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId,
                          row_height);
    auto subtitle_label = std::make_unique<views::Label>(
        subtitle, views::style::CONTEXT_BUTTON, STYLE_SECONDARY);
    subtitle_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    subtitle_label->SetAutoColorReadabilityEnabled(false);
    grid_layout->SkipColumns(1);
    subtitle_ = grid_layout->AddView(std::move(subtitle_label));
  }

  SetTooltipAndAccessibleName(this, title_, subtitle_, GetLocalBounds(),
                              taken_width_, auto_compute_tooltip_);

  SetBorder(CreateBorderWithVerticalSpacing(remaining_vert_spacing));
}

HoverButton::~HoverButton() {}

bool HoverButton::OnKeyPressed(const ui::KeyEvent& event) {
  // Unlike MenuButton, HoverButton should not be activated when the up or down
  // arrow key is pressed.
  if (event.key_code() == ui::VKEY_UP || event.key_code() == ui::VKEY_DOWN)
    return false;
  return MenuButton::OnKeyPressed(event);
}

void HoverButton::SetBorder(std::unique_ptr<views::Border> b) {
  MenuButton::SetBorder(std::move(b));
  // Make sure the minimum size is correct according to the layout (if any).
  if (GetLayoutManager())
    SetMinSize(GetLayoutManager()->GetPreferredSize(this));
}

void HoverButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);
}

gfx::Insets HoverButton::GetInsets() const {
  if (insets_)
    return insets_.value();
  return views::MenuButton::GetInsets();
}

void HoverButton::SetSubtitleElideBehavior(gfx::ElideBehavior elide_behavior) {
  if (subtitle_ && !subtitle_->GetText().empty())
    subtitle_->SetElideBehavior(elide_behavior);
}

void HoverButton::SetTitleTextWithHintRange(const base::string16& title_text,
                                            const gfx::Range& range) {
  DCHECK(title_);
  title_->SetText(title_text);

  if (range.IsValid()) {
    views::StyledLabel::RangeStyleInfo style_info;
    style_info.text_style = STYLE_SECONDARY;
    title_->AddStyleRange(range, style_info);
  }
  title_->SizeToFit(0);
  SetTooltipAndAccessibleName(this, title_, subtitle_, GetLocalBounds(),
                              taken_width_, auto_compute_tooltip_);
}

views::Button::KeyClickAction HoverButton::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN) {
    // As the hover button is presented in the user menu, it triggers an
    // |CLICK_ON_KEY_PRESS| action every time the user clicks on enter on all
    // platforms (it ignores the value of
    // |PlatformStyle::kReturnClicksFocusedControl|.
    return CLICK_ON_KEY_PRESS;
  }
  return MenuButton::GetKeyClickActionForEvent(event);
}

void HoverButton::StateChanged(ButtonState old_state) {
  MenuButton::StateChanged(old_state);

  // |HoverButtons| are designed for use in a list, so ensure only one button
  // can have a hover background at any time by requesting focus on hover.
  if (state() == STATE_HOVERED && old_state != STATE_PRESSED) {
    RequestFocus();
  } else if (state() == STATE_NORMAL && HasFocus()) {
    GetFocusManager()->SetFocusedView(nullptr);
  }
}

SkColor HoverButton::GetInkDropBaseColor() const {
  return views::style::GetColor(*this, views::style::CONTEXT_BUTTON,
                                STYLE_SECONDARY);
}

std::unique_ptr<views::InkDrop> HoverButton::CreateInkDrop() {
  std::unique_ptr<views::InkDrop> ink_drop = MenuButton::CreateInkDrop();
  // Turn on highlighting when the button is focused only - hovering the button
  // will request focus.
  ink_drop->SetShowHighlightOnFocus(true);
  ink_drop->SetShowHighlightOnHover(false);
  return ink_drop;
}

void HoverButton::Layout() {
  MenuButton::Layout();

  // Vertically center |title_| manually since it doesn't have a LayoutManager.
  if (title_) {
    DCHECK(title_->parent());
    int y_center = title_->parent()->height() / 2 - title_->size().height() / 2;
    title_->SetPosition(gfx::Point(title_->x(), y_center));
  }
}

views::View* HoverButton::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (!HitTestPoint(point))
    return nullptr;

  // Let the secondary control handle it if it has a tooltip.
  if (secondary_view_) {
    gfx::Point point_in_secondary_view(point);
    ConvertPointToTarget(this, secondary_view_, &point_in_secondary_view);
    View* handler =
        secondary_view_->GetTooltipHandlerForPoint(point_in_secondary_view);
    if (handler) {
      gfx::Point point_in_handler_view(point);
      ConvertPointToTarget(this, handler, &point_in_handler_view);
      if (!handler->GetTooltipText(point_in_secondary_view).empty()) {
        return handler;
      }
    }
  }

  // If possible, take advantage of the |views::Label| tooltip behavior, which
  // only sets the tooltip when the text is too long.
  if (title_)
    return this;
  return label();
}

void HoverButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // HoverButtons use a rectangular highlight to encompass the full width of
  // their parent.
  auto path = std::make_unique<SkPath>();
  path->addRect(RectToSkRect(GetLocalBounds()));
  SetProperty(views::kHighlightPathKey, path.release());

  if (title_) {
    SetTooltipAndAccessibleName(this, title_, subtitle_, GetLocalBounds(),
                                taken_width_, auto_compute_tooltip_);
  }
}

void HoverButton::SetStyle(Style style) {
  if (style == STYLE_PROMINENT) {
    SkColor background_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
    SetBackground(views::CreateSolidBackground(background_color));
    // White text on |gfx::kGoogleBlue500| would be adjusted by
    // AutoColorRedability. However, this specific combination has an
    // exception (http://go/mdcontrast). So, disable AutoColorReadability.
    title_->SetAutoColorReadabilityEnabled(false);
    SetTitleTextStyle(views::style::STYLE_DIALOG_BUTTON_DEFAULT,
                      background_color);
    SetSubtitleColor(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_TextOnProminentButtonColor));
  } else if (style == STYLE_ERROR) {
    DCHECK_EQ(nullptr, background());
    title_->SetDefaultTextStyle(STYLE_RED);
  } else {
    NOTREACHED();
  }
}

void HoverButton::SetTitleTextStyle(views::style::TextStyle text_style,
                                    SkColor background_color) {
  title_->SetDisplayedOnBackgroundColor(background_color);
  title_->SetDefaultTextStyle(text_style);
}

void HoverButton::SetSubtitleColor(SkColor color) {
  if (subtitle_)
    subtitle_->SetEnabledColor(color);
}

void HoverButton::OnMenuButtonClicked(Button* source,
                                      const gfx::Point& point,
                                      const ui::Event* event) {
  if (listener_)
    listener_->ButtonPressed(source, *event);
}
