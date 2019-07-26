// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

#include <utility>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// TODO(markusheintz|msw): Use NativeTheme colors.
constexpr SkColor kTabTitleColor_InactiveBorder =
    SkColorSetARGB(0xFF, 0x64, 0x64, 0x64);
constexpr SkColor kTabTitleColor_InactiveHighlight = gfx::kGoogleGrey700;
constexpr SkColor kTabTitleColor_ActiveBorder = SK_ColorBLACK;
constexpr SkColor kTabTitleColor_ActiveHighlight = gfx::kGoogleBlue600;
constexpr SkColor kTabTitleColor_Hovered = SK_ColorBLACK;
constexpr SkColor kTabBorderColor = SkColorSetRGB(0xC8, 0xC8, 0xC8);
constexpr SkScalar kTabBorderThickness = 1.0f;
constexpr SkColor kTabHighlightBackgroundColor_Active =
    SkColorSetARGB(0xFF, 0xE8, 0xF0, 0xFE);
constexpr SkColor kTabHighlightBackgroundColor_Focused =
    SkColorSetARGB(0xFF, 0xD2, 0xE3, 0xFC);
constexpr int kTabHighlightBorderRadius = 32;
constexpr int kTabHighlightPreferredHeight = 32;
constexpr int kTabHighlightPreferredWidth = 192;

constexpr gfx::Font::Weight kHoverWeightBorder = gfx::Font::Weight::NORMAL;
constexpr gfx::Font::Weight kHoverWeightHighlight = gfx::Font::Weight::MEDIUM;
constexpr gfx::Font::Weight kActiveWeight = gfx::Font::Weight::BOLD;
constexpr gfx::Font::Weight kInactiveWeightBorder = gfx::Font::Weight::NORMAL;
constexpr gfx::Font::Weight kInactiveWeightHighlight =
    gfx::Font::Weight::MEDIUM;

constexpr int kLabelFontSizeDeltaHighlight = 1;

constexpr int kHarmonyTabStripTabHeight = 32;
constexpr int kBorderThickness = 2;

}  // namespace

// A subclass of Tab that implements the Harmony visual styling.
class MdTab : public Tab {
 public:
  MdTab(TabbedPane* tabbed_pane, const base::string16& title, View* contents);
  ~MdTab() override;

  // Overridden from Tab:
  void OnStateChanged() override;

  // Overridden from View:
  gfx::Size CalculatePreferredSize() const override;
  void OnFocus() override;
  void OnBlur() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MdTab);
};

// A subclass of TabStrip that implements the Harmony visual styling. This
// class uses a BoxLayout to position tabs.
class MdTabStrip : public TabStrip, public gfx::AnimationDelegate {
 public:
  MdTabStrip(TabbedPane::Orientation orientation,
             TabbedPane::TabStripStyle style);
  ~MdTabStrip() override;

  // Overridden from TabStrip:
  void OnSelectedTabChanged(Tab* from_tab, Tab* to_tab) override;

  // Overridden from View:
  void OnPaintBorder(gfx::Canvas* canvas) override;

  // Overridden from AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  // Animations for expanding and contracting the selection bar. When changing
  // selections, the selection bar first grows to encompass both the old and new
  // selections, then shrinks to encompass only the new selection. The rates of
  // expansion and contraction each follow the cubic bezier curves used in
  // gfx::Tween; see MdTabStrip::OnPaintBorder for details.
  std::unique_ptr<gfx::LinearAnimation> expand_animation_;
  std::unique_ptr<gfx::LinearAnimation> contract_animation_;

  // The x-coordinate ranges of the old selection and the new selection.
  gfx::Range animating_from_;
  gfx::Range animating_to_;

  DISALLOW_COPY_AND_ASSIGN(MdTabStrip);
};


Tab::Tab(TabbedPane* tabbed_pane, const base::string16& title, View* contents)
    : tabbed_pane_(tabbed_pane),
      state_(State::kActive),
      contents_(contents) {
  // Calculate the size while the font list is bold.
  auto title_label = std::make_unique<Label>(title, style::CONTEXT_LABEL,
                                             style::STYLE_TAB_ACTIVE);
  title_ = title_label.get();
  preferred_title_size_ = title_label->GetPreferredSize();
  const bool is_vertical =
      tabbed_pane_->GetOrientation() == TabbedPane::Orientation::kVertical;
  const bool is_highlight_style =
      tabbed_pane_->GetStyle() == TabbedPane::TabStripStyle::kHighlight;

  if (is_vertical)
    title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  if (is_highlight_style && is_vertical) {
    constexpr int kTabVerticalPadding = 8;
    constexpr int kTabHorizontalPadding = 32;
    SetBorder(CreateEmptyBorder(gfx::Insets(
        kTabVerticalPadding, kTabHorizontalPadding, kTabVerticalPadding, 0)));
  } else {
    constexpr int kTabVerticalPadding = 5;
    constexpr int kTabHorizontalPadding = 10;
    SetBorder(CreateEmptyBorder(
        gfx::Insets(kTabVerticalPadding, kTabHorizontalPadding)));
  }
  SetLayoutManager(std::make_unique<FillLayout>());
  SetState(State::kInactive);
  // Calculate the size while the font list is normal and set the max size.
  preferred_title_size_.SetToMax(title_label->GetPreferredSize());
  AddChildView(std::move(title_label));

  // Use leaf so that name is spoken by screen reader without exposing the
  // children.
  GetViewAccessibility().OverrideIsLeaf(true);
}

Tab::~Tab() = default;

void Tab::SetSelected(bool selected) {
  contents_->SetVisible(selected);
  SetState(selected ? State::kActive : State::kInactive);
#if defined(OS_MACOSX)
  SetFocusBehavior(selected ? FocusBehavior::ACCESSIBLE_ONLY
                            : FocusBehavior::NEVER);
#else
  SetFocusBehavior(selected ? FocusBehavior::ALWAYS : FocusBehavior::NEVER);
#endif
}

const base::string16& Tab::GetTitleText() const {
  return title_->GetText();
}

void Tab::SetTitleText(const base::string16& text) {
  title_->SetText(text);

  // Active and inactive states use different font sizes. Find the largest size
  // and reserve that amount of space.
  State old_state = state_;
  SetState(State::kActive);
  preferred_title_size_ = GetPreferredSize();
  SetState(State::kInactive);
  preferred_title_size_.SetToMax(GetPreferredSize());
  SetState(old_state);

  InvalidateLayout();
}

void Tab::OnStateChanged() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const bool is_highlight_mode =
      tabbed_pane_->GetStyle() == TabbedPane::TabStripStyle::kHighlight;
  const int font_size_delta = is_highlight_mode ? kLabelFontSizeDeltaHighlight
                                                : ui::kLabelFontSizeDelta;
  switch (state_) {
    case State::kInactive:
      // Notify assistive tools to update this tab's selected status.
      // The way Chrome OS accessibility is implemented right now, firing almost
      // any event will work, we just need to trigger its state to be refreshed.
      NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged, true);
      title_->SetEnabledColor(is_highlight_mode
                                  ? kTabTitleColor_InactiveHighlight
                                  : kTabTitleColor_InactiveBorder);
      title_->SetFontList(
          rb.GetFontListWithDelta(font_size_delta, gfx::Font::NORMAL,
                                  is_highlight_mode ? kInactiveWeightHighlight
                                                    : kInactiveWeightBorder));
      break;
    case State::kActive:
      title_->SetEnabledColor(is_highlight_mode ? kTabTitleColor_ActiveHighlight
                                                : kTabTitleColor_ActiveBorder);
      title_->SetFontList(rb.GetFontListWithDelta(
          font_size_delta, gfx::Font::NORMAL, kActiveWeight));
      break;
    case State::kHovered:
      title_->SetEnabledColor(kTabTitleColor_Hovered);
      title_->SetFontList(rb.GetFontListWithDelta(
          font_size_delta, gfx::Font::NORMAL,
          is_highlight_mode ? kHoverWeightHighlight : kHoverWeightBorder));
      break;
  }
}

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  if (GetEnabled() && event.IsOnlyLeftMouseButton())
    tabbed_pane_->SelectTab(this);
  return true;
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  SetState(selected() ? State::kActive : State::kHovered);
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  SetState(selected() ? State::kActive : State::kInactive);
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      // Fallthrough.
    case ui::ET_GESTURE_TAP:
      // SelectTab also sets the right tab color.
      tabbed_pane_->SelectTab(this);
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
      SetState(selected() ? State::kActive : State::kInactive);
      break;
    default:
      break;
  }
  event->SetHandled();
}

gfx::Size Tab::CalculatePreferredSize() const {
  gfx::Size size(preferred_title_size_);
  size.Enlarge(GetInsets().width(), GetInsets().height());
  if (tabbed_pane_->GetStyle() == TabbedPane::TabStripStyle::kHighlight &&
      tabbed_pane_->GetOrientation() == TabbedPane::Orientation::kVertical) {
    size.SetToMax(
        gfx::Size(kTabHighlightPreferredWidth, kTabHighlightPreferredHeight));
  }
  return size;
}

void Tab::SetState(State state) {
  if (state == state_)
    return;
  state_ = state;
  OnStateChanged();
  SchedulePaint();
}

void Tab::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (!selected())
    return;
  if (tabbed_pane_->GetOrientation() != TabbedPane::Orientation::kVertical ||
      tabbed_pane_->GetStyle() != TabbedPane::TabStripStyle::kHighlight) {
    return;
  }

  SkScalar radius = SkIntToScalar(kTabHighlightBorderRadius);
  SkPath path;
  gfx::Rect bounds(size());
  if (base::i18n::IsRTL()) {
    const SkScalar kRadius[8] = {radius, radius, 0, 0, 0, 0, radius, radius};
    path.addRoundRect(gfx::RectToSkRect(bounds), kRadius);
  } else {
    const SkScalar kRadius[8] = {0, 0, radius, radius, radius, radius, 0, 0};
    path.addRoundRect(gfx::RectToSkRect(bounds), kRadius);
  }

  cc::PaintFlags fill_flags;
  fill_flags.setAntiAlias(true);
  if (HasFocus())
    fill_flags.setColor(kTabHighlightBackgroundColor_Focused);
  else
    fill_flags.setColor(kTabHighlightBackgroundColor_Active);
  canvas->DrawPath(path, fill_flags);
}

void Tab::GetAccessibleNodeData(ui::AXNodeData* data) {
  data->role = ax::mojom::Role::kTab;
  data->SetName(title()->GetText());
  data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, selected());
}

bool Tab::HandleAccessibleAction(const ui::AXActionData& action_data) {
  // If the assistive tool sends kSetSelection, handle it like kDoDefault.
  // These generate a click event handled in Tab::OnMousePressed.
  ui::AXActionData action_data_copy(action_data);
  if (action_data.action == ax::mojom::Action::kSetSelection)
    action_data_copy.action = ax::mojom::Action::kDoDefault;
  return View::HandleAccessibleAction(action_data_copy);
}

void Tab::OnFocus() {
  OnStateChanged();
  // When the tab gains focus, send an accessibility event indicating that the
  // contents are focused. When the tab loses focus, whichever new View ends up
  // with focus will send an ax::mojom::Event::kFocus of its own, so there's no
  // need to send one in OnBlur().
  if (contents())
    contents()->NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
  SchedulePaint();
}

void Tab::OnBlur() {
  OnStateChanged();
  SchedulePaint();
}

bool Tab::OnKeyPressed(const ui::KeyEvent& event) {
  ui::KeyboardCode key = event.key_code();
  const bool is_horizontal =
      tabbed_pane_->GetOrientation() == TabbedPane::Orientation::kHorizontal;
  // Use left and right arrows to navigate tabs in horizontal orientation.
  if (is_horizontal) {
    return (key == ui::VKEY_LEFT || key == ui::VKEY_RIGHT) &&
           tabbed_pane_->MoveSelectionBy(key == ui::VKEY_RIGHT ? 1 : -1);
  }
  // Use up and down arrows to navigate tabs in vertical orientation.
  return (key == ui::VKEY_UP || key == ui::VKEY_DOWN) &&
         tabbed_pane_->MoveSelectionBy(key == ui::VKEY_DOWN ? 1 : -1);
}

BEGIN_METADATA(Tab)
METADATA_PARENT_CLASS(View)
END_METADATA()

MdTab::MdTab(TabbedPane* tabbed_pane,
             const base::string16& title,
             View* contents)
    : Tab(tabbed_pane, title, contents) {
  if (tabbed_pane->GetOrientation() == TabbedPane::Orientation::kHorizontal) {
    SetBorder(CreateEmptyBorder(gfx::Insets(kBorderThickness)));
  }
  OnStateChanged();
}

MdTab::~MdTab() = default;

void MdTab::OnStateChanged() {
  // kHighlight mode has different color theme.
  if (tabbed_pane()->GetStyle() == TabbedPane::TabStripStyle::kHighlight) {
    Tab::OnStateChanged();
    return;
  }

  ui::NativeTheme* theme = GetNativeTheme();

  SkColor font_color =
      selected()
          ? theme->GetSystemColor(ui::NativeTheme::kColorId_TabTitleColorActive)
          : theme->GetSystemColor(
                ui::NativeTheme::kColorId_TabTitleColorInactive);
  title()->SetEnabledColor(font_color);

  gfx::Font::Weight font_weight = gfx::Font::Weight::MEDIUM;
#if defined(OS_WIN)
  if (selected())
    font_weight = gfx::Font::Weight::BOLD;
#endif

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title()->SetFontList(rb.GetFontListWithDelta(ui::kLabelFontSizeDelta,
                                               gfx::Font::NORMAL, font_weight));
}

gfx::Size MdTab::CalculatePreferredSize() const {
  return gfx::Size(Tab::CalculatePreferredSize().width(),
                   kHarmonyTabStripTabHeight);
}

void MdTab::OnFocus() {
  // Do not draw focus ring in kHighlight mode.
  if (tabbed_pane()->GetStyle() != TabbedPane::TabStripStyle::kHighlight) {
    SetBorder(CreateSolidBorder(
        GetInsets().top(), GetNativeTheme()->GetSystemColor(
                               ui::NativeTheme::kColorId_FocusedBorderColor)));
  }

  // When the tab gains focus, send an accessibility event indicating that the
  // contents are focused. When the tab loses focus, whichever new View ends up
  // with focus will send an ax::mojom::Event::kFocus of its own, so there's no
  // need to send one in OnBlur().
  if (contents())
    contents()->NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
  SchedulePaint();
}

void MdTab::OnBlur() {
  // Do not draw focus ring in kHighlight mode.
  if (tabbed_pane()->GetStyle() != TabbedPane::TabStripStyle::kHighlight)
    SetBorder(CreateEmptyBorder(GetInsets()));
  SchedulePaint();
}

// static
constexpr size_t TabStrip::kNoSelectedTab;

TabStrip::TabStrip(TabbedPane::Orientation orientation,
                   TabbedPane::TabStripStyle style)
    : orientation_(orientation), style_(style) {
  std::unique_ptr<BoxLayout> layout;
  if (orientation == TabbedPane::Orientation::kHorizontal) {
    constexpr int kTabStripLeadingEdgePadding = 9;
    layout = std::make_unique<BoxLayout>(
        BoxLayout::Orientation::kHorizontal,
        gfx::Insets(0, kTabStripLeadingEdgePadding));
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kEnd);
  } else {
    constexpr int kTabStripEdgePadding = 8;
    constexpr int kTabSpacing = 8;
    layout = std::make_unique<BoxLayout>(
        BoxLayout::Orientation::kVertical,
        gfx::Insets(kTabStripEdgePadding, 0, 0, 0), kTabSpacing);
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStart);
  }
  layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kStart);
  layout->SetDefaultFlex(0);
  SetLayoutManager(std::move(layout));

  GetViewAccessibility().OverrideRole(ax::mojom::Role::kIgnored);
}

TabStrip::~TabStrip() = default;

void TabStrip::OnSelectedTabChanged(Tab* from_tab, Tab* to_tab) {}

void TabStrip::OnPaintBorder(gfx::Canvas* canvas) {
  // Do not draw border line in kHighlight mode.
  if (style_ == TabbedPane::TabStripStyle::kHighlight)
    return;

  cc::PaintFlags fill_flags;
  fill_flags.setColor(kTabBorderColor);
  fill_flags.setStrokeWidth(kTabBorderThickness);
  SkScalar line_center_cross_axis;
  SkScalar line_end_main_axis;

  const bool is_horizontal =
      orientation_ == TabbedPane::Orientation::kHorizontal;
  if (is_horizontal) {
    line_center_cross_axis =
        SkIntToScalar(height()) - (kTabBorderThickness / 2);
    line_end_main_axis = SkIntToScalar(width());
  } else {
    line_center_cross_axis = SkIntToScalar(width()) - (kTabBorderThickness / 2);
    line_end_main_axis = SkIntToScalar(height());
  }

  size_t selected_tab_index = GetSelectedTabIndex();
  if (selected_tab_index == kNoSelectedTab) {
    if (is_horizontal) {
      canvas->sk_canvas()->drawLine(0, line_center_cross_axis,
                                    line_end_main_axis, line_center_cross_axis,
                                    fill_flags);
    } else {
      canvas->sk_canvas()->drawLine(line_center_cross_axis, 0,
                                    line_center_cross_axis, line_end_main_axis,
                                    fill_flags);
    }
  } else {
    Tab* selected_tab = GetTabAtIndex(selected_tab_index);
    SkPath path;
    SkScalar tab_height =
        SkIntToScalar(selected_tab->height()) - kTabBorderThickness;
    SkScalar tab_width;

    SkScalar tab_start_x = SkIntToScalar(selected_tab->GetMirroredX());
    SkScalar tab_start_y = SkIntToScalar(selected_tab->bounds().y());
    if (is_horizontal) {
      tab_width = SkIntToScalar(selected_tab->width()) - kTabBorderThickness;
      path.moveTo(0, line_center_cross_axis);
      path.rLineTo(tab_start_x, 0);
      path.rLineTo(0, -tab_height);
      path.rLineTo(tab_width, 0);
      path.rLineTo(0, tab_height);
      path.lineTo(line_end_main_axis, line_center_cross_axis);
    } else {
      tab_width =
          SkIntToScalar(width() - selected_tab->GetInsets().left() / 2) -
          kTabBorderThickness;
      path.moveTo(line_center_cross_axis, 0);
      path.rLineTo(0, tab_start_y);
      path.rLineTo(-tab_width, 0);
      path.rLineTo(0, tab_height);
      path.rLineTo(tab_width, 0);
      path.lineTo(line_center_cross_axis, line_end_main_axis);
    }

    fill_flags.setStyle(cc::PaintFlags::kStroke_Style);
    canvas->DrawPath(path, fill_flags);
  }
}

Tab* TabStrip::GetTabAtIndex(size_t index) const {
  DCHECK_LT(index, children().size());
  return static_cast<Tab*>(children()[index]);
}

size_t TabStrip::GetSelectedTabIndex() const {
  for (size_t i = 0; i < children().size(); ++i)
    if (GetTabAtIndex(i)->selected())
      return i;
  return kNoSelectedTab;
}

Tab* TabStrip::GetSelectedTab() const {
  size_t index = GetSelectedTabIndex();
  return index == kNoSelectedTab ? nullptr : GetTabAtIndex(index);
}

Tab* TabStrip::GetTabAtDeltaFromSelected(int delta) const {
  const size_t selected_tab_index = GetSelectedTabIndex();
  DCHECK_NE(kNoSelectedTab, selected_tab_index);
  const size_t num_children = children().size();
  // Clamping |delta| here ensures that even a large negative |delta| will not
  // cause the addition in the next statement to wrap below 0.
  delta %= static_cast<int>(num_children);
  return GetTabAtIndex((selected_tab_index + num_children + delta) %
                       num_children);
}

TabbedPane::Orientation TabStrip::GetOrientation() const {
  return orientation_;
}

TabbedPane::TabStripStyle TabStrip::GetStyle() const {
  return style_;
}

DEFINE_ENUM_CONVERTERS(TabbedPane::Orientation,
                       {TabbedPane::Orientation::kHorizontal,
                        base::ASCIIToUTF16("HORIZONTAL")},
                       {TabbedPane::Orientation::kVertical,
                        base::ASCIIToUTF16("VERTICAL")})

DEFINE_ENUM_CONVERTERS(TabbedPane::TabStripStyle,
                       {TabbedPane::TabStripStyle::kBorder,
                        base::ASCIIToUTF16("BORDER")},
                       {TabbedPane::TabStripStyle::kHighlight,
                        base::ASCIIToUTF16("HIGHLIGHT")})

BEGIN_METADATA(TabStrip)
METADATA_PARENT_CLASS(View)
ADD_READONLY_PROPERTY_METADATA(TabStrip, int, SelectedTabIndex)
ADD_READONLY_PROPERTY_METADATA(TabStrip, TabbedPane::Orientation, Orientation)
ADD_READONLY_PROPERTY_METADATA(TabStrip, TabbedPane::TabStripStyle, Style)
END_METADATA()

MdTabStrip::MdTabStrip(TabbedPane::Orientation orientation,
                       TabbedPane::TabStripStyle style)
    : TabStrip(orientation, style) {
  if (orientation == TabbedPane::Orientation::kHorizontal) {
    auto layout =
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
    layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);
    layout->SetDefaultFlex(1);
    SetLayoutManager(std::move(layout));
  }

  // These durations are taken from the Paper Tabs source:
  // https://github.com/PolymerElements/paper-tabs/blob/master/paper-tabs.html
  // See |selectionBar.expand| and |selectionBar.contract|.
  expand_animation_ = std::make_unique<gfx::LinearAnimation>(this);
  expand_animation_->SetDuration(base::TimeDelta::FromMilliseconds(150));

  contract_animation_ = std::make_unique<gfx::LinearAnimation>(this);
  contract_animation_->SetDuration(base::TimeDelta::FromMilliseconds(180));
}

MdTabStrip::~MdTabStrip() = default;

void MdTabStrip::OnSelectedTabChanged(Tab* from_tab, Tab* to_tab) {
  DCHECK(!from_tab->selected());
  DCHECK(to_tab->selected());

  if (GetOrientation() == TabbedPane::Orientation::kHorizontal) {
    animating_from_ = gfx::Range(from_tab->GetMirroredX(),
                                 from_tab->GetMirroredX() + from_tab->width());
    animating_to_ = gfx::Range(to_tab->GetMirroredX(),
                               to_tab->GetMirroredX() + to_tab->width());
  } else {
    animating_from_ = gfx::Range(from_tab->bounds().y(),
                                 from_tab->bounds().y() + from_tab->height());
    animating_to_ = gfx::Range(to_tab->bounds().y(),
                               to_tab->bounds().y() + to_tab->height());
  }

  contract_animation_->Stop();
  expand_animation_->Start();
}

void MdTabStrip::OnPaintBorder(gfx::Canvas* canvas) {
  // Do not draw border line in kHighlight mode.
  if (GetStyle() == TabbedPane::TabStripStyle::kHighlight)
    return;

  constexpr int kUnselectedBorderThickness = 1;
  constexpr int kSelectedBorderThickness = 2;
  const bool is_horizontal =
      GetOrientation() == TabbedPane::Orientation::kHorizontal;

  int max_cross_axis;

  // First, draw the unselected border across the TabStrip's entire width or
  // height, depending on the orientation of the tab alignment. The area
  // underneath or on the right of the selected tab will be overdrawn later.
  gfx::Rect rect;
  if (is_horizontal) {
    max_cross_axis = children().front()->bounds().bottom();
    rect = gfx::Rect(0, max_cross_axis - kUnselectedBorderThickness, width(),
                     kUnselectedBorderThickness);
  } else {
    max_cross_axis = width();
    rect = gfx::Rect(max_cross_axis - kUnselectedBorderThickness, 0,
                     kUnselectedBorderThickness, height());
  }
  canvas->FillRect(rect, GetNativeTheme()->GetSystemColor(
                             ui::NativeTheme::kColorId_TabBottomBorder));

  int min_main_axis = 0;
  int max_main_axis = 0;

  // Now, figure out the range to draw the selection marker underneath. There
  // are three states here:
  // 1) Expand animation is running: use FAST_OUT_LINEAR_IN to grow the
  //    selection marker until it encompasses both the previously selected tab
  //    and the currently selected tab;
  // 2) Contract animation is running: use LINEAR_OUT_SLOW_IN to shrink the
  //    selection marker until it encompasses only the currently selected tab;
  // 3) No animations running: the selection marker is only under the currently
  //    selected tab.
  Tab* tab = GetSelectedTab();
  if (!tab)
    return;
  if (expand_animation_->is_animating()) {
    bool animating_leading = animating_to_.start() < animating_from_.start();
    double anim_value = gfx::Tween::CalculateValue(
        gfx::Tween::FAST_OUT_LINEAR_IN, expand_animation_->GetCurrentValue());

    if (animating_leading) {
      min_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.start(), animating_to_.start());
      max_main_axis = animating_from_.end();
    } else {
      min_main_axis = animating_from_.start();
      max_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.end(), animating_to_.end());
    }
  } else if (contract_animation_->is_animating()) {
    bool animating_leading = animating_to_.start() < animating_from_.start();
    double anim_value = gfx::Tween::CalculateValue(
        gfx::Tween::LINEAR_OUT_SLOW_IN, contract_animation_->GetCurrentValue());
    if (animating_leading) {
      min_main_axis = animating_to_.start();
      max_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.end(), animating_to_.end());
    } else {
      min_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.start(), animating_to_.start());
      max_main_axis = animating_to_.end();
    }
  } else if (tab) {
    if (is_horizontal) {
      min_main_axis = tab->GetMirroredX();
      max_main_axis = tab->GetMirroredX() + tab->width();
    } else {
      min_main_axis = tab->bounds().y();
      max_main_axis = tab->bounds().y() + tab->height();
    }
  }

  DCHECK(min_main_axis != max_main_axis);
  // Draw over the unselected border from above.
  if (is_horizontal) {
    rect = gfx::Rect(min_main_axis, max_cross_axis - kSelectedBorderThickness,
                     max_main_axis - min_main_axis, kSelectedBorderThickness);
  } else {
    rect = gfx::Rect(max_cross_axis - kSelectedBorderThickness, min_main_axis,
                     kSelectedBorderThickness, max_main_axis - min_main_axis);
  }
  canvas->FillRect(
      rect, SkColorSetA(GetNativeTheme()->GetSystemColor(
                            ui::NativeTheme::kColorId_FocusedBorderColor),
                        SK_AlphaOPAQUE));
}

void MdTabStrip::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

void MdTabStrip::AnimationEnded(const gfx::Animation* animation) {
  if (animation == expand_animation_.get())
    contract_animation_->Start();
}

TabbedPane::TabbedPane(TabbedPane::Orientation orientation,
                       TabbedPane::TabStripStyle style) {
  DCHECK(orientation != TabbedPane::Orientation::kHorizontal ||
         style != TabbedPane::TabStripStyle::kHighlight);
  tab_strip_ = AddChildView(std::make_unique<MdTabStrip>(orientation, style));
  contents_ = AddChildView(std::make_unique<View>());
}

TabbedPane::~TabbedPane() = default;

size_t TabbedPane::GetSelectedTabIndex() const {
  return tab_strip_->GetSelectedTabIndex();
}

size_t TabbedPane::GetTabCount() {
  DCHECK_EQ(tab_strip_->children().size(), contents_->children().size());
  return contents_->children().size();
}

void TabbedPane::AddTabInternal(size_t index,
                                const base::string16& title,
                                std::unique_ptr<View> contents) {
  DCHECK_LE(index, GetTabCount());
  contents->SetVisible(false);
  contents->GetViewAccessibility().OverrideName(title);
  contents->GetViewAccessibility().OverrideRole(ax::mojom::Role::kTab);

  tab_strip_->AddChildViewAt(
      std::make_unique<MdTab>(this, title, contents.get()),
      static_cast<int>(index));
  contents_->AddChildViewAt(std::move(contents), static_cast<int>(index));
  if (!GetSelectedTab())
    SelectTabAt(index);

  PreferredSizeChanged();
}

void TabbedPane::SelectTab(Tab* new_selected_tab) {
  Tab* old_selected_tab = tab_strip_->GetSelectedTab();
  if (old_selected_tab == new_selected_tab)
    return;

  new_selected_tab->SetSelected(true);
  if (old_selected_tab) {
    if (old_selected_tab->HasFocus())
      new_selected_tab->RequestFocus();
    old_selected_tab->SetSelected(false);
    tab_strip_->OnSelectedTabChanged(old_selected_tab, new_selected_tab);
  }
  tab_strip_->SchedulePaint();

  FocusManager* focus_manager = new_selected_tab->contents()->GetFocusManager();
  if (focus_manager) {
    const View* focused_view = focus_manager->GetFocusedView();
    if (focused_view && contents_->Contains(focused_view) &&
        !new_selected_tab->contents()->Contains(focused_view))
      focus_manager->SetFocusedView(new_selected_tab->contents());
  }

  if (listener())
    listener()->TabSelectedAt(tab_strip_->GetIndexOf(new_selected_tab));
}

void TabbedPane::SelectTabAt(size_t index) {
  Tab* tab = tab_strip_->GetTabAtIndex(index);
  if (tab)
    SelectTab(tab);
}

gfx::Size TabbedPane::CalculatePreferredSize() const {
  gfx::Size size;
  for (const View* child : contents_->children())
    size.SetToMax(child->GetPreferredSize());
  if (GetOrientation() == Orientation::kHorizontal)
    size.Enlarge(0, tab_strip_->GetPreferredSize().height());
  else
    size.Enlarge(tab_strip_->GetPreferredSize().width(), 0);
  return size;
}

TabbedPane::Orientation TabbedPane::GetOrientation() const {
  return tab_strip_->GetOrientation();
}

TabbedPane::TabStripStyle TabbedPane::GetStyle() const {
  return tab_strip_->GetStyle();
}

Tab* TabbedPane::GetTabAt(size_t index) {
  return tab_strip_->GetTabAtIndex(index);
}

Tab* TabbedPane::GetSelectedTab() {
  return tab_strip_->GetSelectedTab();
}

View* TabbedPane::GetSelectedTabContentView() {
  return GetSelectedTab() ? GetSelectedTab()->contents() : nullptr;
}

bool TabbedPane::MoveSelectionBy(int delta) {
  if (contents_->children().size() <= 1)
    return false;
  SelectTab(tab_strip_->GetTabAtDeltaFromSelected(delta));
  return true;
}

void TabbedPane::Layout() {
  const gfx::Size size = tab_strip_->GetPreferredSize();
  if (GetOrientation() == Orientation::kHorizontal) {
    tab_strip_->SetBounds(0, 0, width(), size.height());
    contents_->SetBounds(0, tab_strip_->bounds().bottom(), width(),
                         std::max(0, height() - size.height()));
  } else {
    tab_strip_->SetBounds(0, 0, size.width(), height());
    contents_->SetBounds(tab_strip_->bounds().width(), 0,
                         std::max(0, width() - size.width()), height());
  }
  for (View* child : contents_->children())
    child->SetSize(contents_->size());
}

void TabbedPane::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add) {
    // Support navigating tabs by Ctrl+Tab and Ctrl+Shift+Tab.
    AddAccelerator(ui::Accelerator(ui::VKEY_TAB,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
    AddAccelerator(ui::Accelerator(ui::VKEY_TAB, ui::EF_CONTROL_DOWN));
  }
}

bool TabbedPane::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Handle Ctrl+Tab and Ctrl+Shift+Tab navigation of pages.
  DCHECK(accelerator.key_code() == ui::VKEY_TAB && accelerator.IsCtrlDown());
  return MoveSelectionBy(accelerator.IsShiftDown() ? -1 : 1);
}

void TabbedPane::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTabList;
  const Tab* const selected_tab = GetSelectedTab();
  if (selected_tab)
    node_data->SetName(selected_tab->GetTitleText());
}

BEGIN_METADATA(TabbedPane)
METADATA_PARENT_CLASS(View)
END_METADATA()

}  // namespace views
