// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {
// Formatting constants
static constexpr int kLineHeightDip = 24;
static constexpr int kNumLines = 2;
static constexpr int kCornerRadiusDip = 8;
static constexpr int kHorizontalMarginsDip = 6;
static constexpr int kVerticalMarginsDip = 8;
static constexpr int kCloseButtonMargin = 4;
static constexpr double kPreferredAnchorWidthPercentage = 0.8;
static constexpr int kMaxWidthDip = 548;
static constexpr int kButtonPaddingDip = 48;
static constexpr int kSideMarginDip = 20;
// 90% opacity.
static constexpr int kCaptionBubbleAlpha = 230;
static constexpr char kPrimaryFont[] = "Roboto";
static constexpr char kSecondaryFont[] = "Arial";
static constexpr char kTertiaryFont[] = "sans-serif";
static constexpr int kFontSizePx = 16;
static constexpr double kDefaultRatioInParentX = 0.5;
static constexpr double kDefaultRatioInParentY = 1;
static constexpr int kErrorImageSizeDip = 20;
static constexpr int kFocusRingInnerInsetDip = 3;
static constexpr int kWidgetDisplacementWithArrowKeyDip = 16;

}  // namespace

namespace captions {
// CaptionBubble implementation of BubbleFrameView. This class takes care
// of making the caption draggable and handling the focus ring when the
// Caption Bubble is focused.
class CaptionBubbleFrameView : public views::BubbleFrameView {
 public:
  explicit CaptionBubbleFrameView(views::View* close_button)
      : views::BubbleFrameView(gfx::Insets(), gfx::Insets()),
        close_button_(close_button) {
    // The focus ring is drawn on CaptionBubbleFrameView because it has the
    // correct bounds, but focused state is taken from the CaptionBubble.
    focus_ring_ = views::FocusRing::Install(this);

    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::FLOAT, views::BubbleBorder::DIALOG_SHADOW,
        gfx::kPlaceholderColor);
    border->SetCornerRadius(kCornerRadiusDip);
#if defined(OS_MACOSX)
    // Inset the border so that there's space to draw a focus ring on Mac
    // without clipping by the system window.
    border->set_insets(border->GetBorderAndShadowInsets() + gfx::Insets(1));
#endif
    gfx::Insets shadow = border->GetBorderAndShadowInsets();
    gfx::Insets padding = gfx::Insets(kFocusRingInnerInsetDip);
    focus_ring_->SetPathGenerator(
        std::make_unique<views::RoundRectHighlightPathGenerator>(
            shadow - padding, kCornerRadiusDip + 2));
    views::BubbleFrameView::SetBubbleBorder(std::move(border));
    focus_ring_->SetHasFocusPredicate([](View* view) {
      auto* frame_view = static_cast<CaptionBubbleFrameView*>(view);
      return frame_view->contents_focused();
    });
  }

  ~CaptionBubbleFrameView() override = default;
  CaptionBubbleFrameView(const CaptionBubbleFrameView&) = delete;
  CaptionBubbleFrameView& operator=(const CaptionBubbleFrameView&) = delete;

  void UpdateFocusRing(bool focused) {
    contents_focused_ = focused;
    focus_ring_->SchedulePaint();
  }

  bool contents_focused() { return contents_focused_; }

  // TODO(crbug.com/1055150): This does not work on Linux because the bubble is
  // not a top-level view, so it doesn't receive events. See crbug.com/1074054
  // for more about why it doesn't work.
  int NonClientHitTest(const gfx::Point& point) override {
    // Outside of the window bounds, do nothing.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    // |point| is in coordinates relative to CaptionBubbleFrameView, i.e.
    // (0,0) is the upper left corner of this view. Convert it to screen
    // coordinates to see whether the close button contains this point.
    gfx::Point point_in_screen =
        GetBoundsInScreen().origin() + gfx::Vector2d(point.x(), point.y());
    if (close_button_->GetBoundsInScreen().Contains(point_in_screen))
      return HTCLOSE;

    // Ensure it's within the BubbleFrameView. This takes into account the
    // rounded corners and drop shadow of the BubbleBorder.
    int hit = views::BubbleFrameView::NonClientHitTest(point);

    // After BubbleFrameView::NonClientHitTest processes the bubble-specific
    // hits such as the rounded corners, it checks hits to the bubble's client
    // view. Any hits to ClientFrameView::NonClientHitTest return HTCLIENT or
    // HTNOWHERE. Override these to return HTCAPTION in order to make the
    // entire widget draggable.
    return (hit == HTCLIENT || hit == HTNOWHERE) ? HTCAPTION : hit;
  }

  void Layout() override {
    views::BubbleFrameView::Layout();
    focus_ring_->Layout();
  }

  const char* GetClassName() const override { return "CaptionBubbleFrameView"; }

 private:
  views::View* close_button_;
  std::unique_ptr<views::FocusRing> focus_ring_;
  bool contents_focused_ = false;
};

CaptionBubble::CaptionBubble(views::View* anchor,
                             base::OnceClosure destroyed_callback)
    : BubbleDialogDelegateView(anchor,
                               views::BubbleBorder::FLOAT,
                               views::BubbleBorder::Shadow::NO_SHADOW),
      destroyed_callback_(std::move(destroyed_callback)),
      ratio_in_parent_x_(kDefaultRatioInParentX),
      ratio_in_parent_y_(kDefaultRatioInParentY) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  DialogDelegate::set_draggable(true);
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  // The CaptionBubble is focusable. It will alert the CaptionBubbleFrameView
  // when its focus changes so that the focus ring can be updated.
  // TODO(crbug.com/1055150): Consider using
  // View::FocusBehavior::ACCESSIBLE_ONLY. However, that does not seem to get
  // OnFocus() and OnBlur() called so we never draw the custom focus ring.
  SetFocusBehavior(View::FocusBehavior::ALWAYS);
}

CaptionBubble::~CaptionBubble() = default;

gfx::Rect CaptionBubble::GetBubbleBounds() {
  // Get the height and width of the full bubble using the superclass method.
  // This includes shadow and insets.
  gfx::Rect original_bounds =
      views::BubbleDialogDelegateView::GetBubbleBounds();

  gfx::Rect anchor_rect = GetAnchorView()->GetBoundsInScreen();
  // Calculate the desired width based on the original bubble's width (which is
  // the max allowed per the spec).
  int min_width = anchor_rect.width() - kSideMarginDip * 2;
  int desired_width = anchor_rect.width() * kPreferredAnchorWidthPercentage;
  int width = std::max(min_width, desired_width);
  if (width > original_bounds.width())
    width = original_bounds.width();
  int height = original_bounds.height();

  // The placement is based on the ratio between the center of the widget and
  // the center of the anchor_rect.
  int target_x =
      anchor_rect.x() + anchor_rect.width() * ratio_in_parent_x_ - width / 2.0;
  int target_y = anchor_rect.y() + anchor_rect.height() * ratio_in_parent_y_ -
                 height / 2.0;
  latest_bounds_ = gfx::Rect(target_x, target_y, width, height);
  latest_anchor_bounds_ = GetAnchorView()->GetBoundsInScreen();
  anchor_rect.Inset(kSideMarginDip, 0, kSideMarginDip, kButtonPaddingDip);
  if (!anchor_rect.Contains(latest_bounds_)) {
    latest_bounds_.AdjustToFit(anchor_rect);
  }
  // If it still doesn't fit after being adjusted to fit, then it is too tall
  // or too wide for the tiny window, and we need to simply hide it. Otherwise,
  // ensure it is shown.
  DCHECK(GetWidget());
  bool can_layout = latest_bounds_.height() >= height;
  if (can_layout != can_layout_) {
    can_layout_ = can_layout;
    UpdateBubbleVisibility();
  }

  return latest_bounds_;
}

void CaptionBubble::OnWidgetBoundsChanged(views::Widget* widget,
                                          const gfx::Rect& new_bounds) {
  DCHECK(GetWidget());
  gfx::Rect widget_bounds = GetWidget()->GetWindowBoundsInScreen();
  gfx::Rect anchor_rect = GetAnchorView()->GetBoundsInScreen();
  if (latest_bounds_ == widget_bounds && latest_anchor_bounds_ == anchor_rect) {
    return;
  }

  if (latest_anchor_bounds_ != anchor_rect) {
    // The window has moved. Reposition the widget within it.
    SizeToContents();
    return;
  }
  // Check the widget which changed size is our widget. It's possible for
  // this to be called when another widget resizes.
  // Also check that our widget is visible. If it is not visible then
  // the user has not explicitly moved it (because the user can't see it),
  // so we should take no action.
  if (widget != GetWidget() || !GetWidget()->IsVisible())
    return;

  // The widget has moved within the window. Recalculate the desired ratio
  // within the parent.
  gfx::Rect bounds_rect = GetAnchorView()->GetBoundsInScreen();
  bounds_rect.Inset(kSideMarginDip, 0, kSideMarginDip, kButtonPaddingDip);

  bool out_of_bounds = false;
  if (!bounds_rect.Contains(widget_bounds)) {
    widget_bounds.AdjustToFit(bounds_rect);
    out_of_bounds = true;
  }

  ratio_in_parent_x_ = (widget_bounds.CenterPoint().x() - anchor_rect.x()) /
                       (1.0 * anchor_rect.width());
  ratio_in_parent_y_ = (widget_bounds.CenterPoint().y() - anchor_rect.y()) /
                       (1.0 * anchor_rect.height());

  if (out_of_bounds)
    SizeToContents();
}

void CaptionBubble::Init() {
  int content_top_bottom_margin = kHorizontalMarginsDip - kCloseButtonMargin;
  int content_sides_margin = kVerticalMarginsDip - kCloseButtonMargin;

  views::View* content_container = new views::View();
  views::FlexLayout* layout = content_container->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  layout->SetInteriorMargin(
      gfx::Insets(content_top_bottom_margin, content_sides_margin));
  layout->SetDefault(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred,
                               /*adjust_height_for_width*/ true));

  views::BoxLayout* main_layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(0), 0));
  main_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);

  // TODO(crbug.com/1055150): Use system caption color scheme rather than
  // hard-coding the colors.
  SkColor caption_bubble_color_ =
      SkColorSetA(gfx::kGoogleGrey900, kCaptionBubbleAlpha);
  set_color(caption_bubble_color_);
  set_close_on_deactivate(false);

  auto label = std::make_unique<views::Label>();
  label->SetMultiLine(true);
  label->SetMaximumWidth(kMaxWidthDip - kVerticalMarginsDip);
  label->SetEnabledColor(SK_ColorWHITE);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetTooltipText(base::string16());

  auto title = std::make_unique<views::Label>();
  title->SetEnabledColor(gfx::kGoogleGrey500);
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  title->SetText(l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_TITLE));

  auto error_message = std::make_unique<views::Label>();
  error_message->SetEnabledColor(SK_ColorWHITE);
  error_message->SetBackgroundColor(SK_ColorTRANSPARENT);
  error_message->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  error_message->SetText(
      l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_ERROR));
  error_message->SetVisible(false);

  auto error_icon = std::make_unique<views::ImageView>();
  error_icon->SetImage(gfx::CreateVectorIcon(
      vector_icons::kErrorOutlineIcon, kErrorImageSizeDip, SK_ColorWHITE));
  error_icon->SetVisible(false);

  auto close_button = views::CreateVectorImageButton(this);
  views::SetImageFromVectorIcon(close_button.get(),
                                vector_icons::kCloseRoundedIcon, SK_ColorWHITE);
  // TODO(crbug.com/1055150): Use a custom string explaining we dismiss from the
  // current tab on close, but leave the feature enabled.
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->SizeToPreferredSize();
  close_button->SetFocusForPlatform();
  views::InstallCircleHighlightPathGenerator(close_button.get());

  set_margins(gfx::Insets(kCloseButtonMargin));

  title_ = content_container->AddChildView(std::move(title));
  label_ = content_container->AddChildView(std::move(label));

  error_icon_ = content_container->AddChildView(std::move(error_icon));
  error_message_ = content_container->AddChildView(std::move(error_message));

  close_button_ = AddChildView(std::move(close_button));
  content_container_ = AddChildView(content_container);

  UpdateTextSize();
}

bool CaptionBubble::ShouldShowCloseButton() const {
  // We draw our own close button so that we could show/hide it when the
  // mouse moves, and so that in the future we can add an expand button.
  return false;
}

views::NonClientFrameView* CaptionBubble::CreateNonClientFrameView(
    views::Widget* widget) {
  frame_ = new CaptionBubbleFrameView(close_button_);
  return frame_;
}

void CaptionBubble::OnKeyEvent(ui::KeyEvent* event) {
  // Use the arrow keys to move.
  if (event->type() == ui::ET_KEY_PRESSED) {
    gfx::Vector2d offset;
    if (event->key_code() == ui::VKEY_UP) {
      offset.set_y(-kWidgetDisplacementWithArrowKeyDip);
    }
    if (event->key_code() == ui::VKEY_DOWN) {
      offset.set_y(kWidgetDisplacementWithArrowKeyDip);
    }
    if (event->key_code() == ui::VKEY_LEFT) {
      offset.set_x(-kWidgetDisplacementWithArrowKeyDip);
    }
    if (event->key_code() == ui::VKEY_RIGHT) {
      offset.set_x(kWidgetDisplacementWithArrowKeyDip);
    }
    if (offset != gfx::Vector2d()) {
      DCHECK(GetWidget());
      gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
      bounds.Offset(offset);
      GetWidget()->SetBounds(bounds);
      return;
    }
  }
  views::BubbleDialogDelegateView::OnKeyEvent(event);
}

bool CaptionBubble::AcceleratorPressed(const ui::Accelerator& accelerator) {
  DCHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  // We don't want to close when the user hits "escape", because this isn't a
  // normal dialog bubble -- it's meant to be up all the time. We just want to
  // release focus back to the page in that case.
  // Users should use the "close" button to close the bubble.
  // TODO(crbug.com/1055150): This doesn't work in Mac.
  GetAnchorView()->RequestFocus();
  return true;
}

void CaptionBubble::OnFocus() {
  frame_->UpdateFocusRing(true);
}

void CaptionBubble::OnBlur() {
  frame_->UpdateFocusRing(false);
}

// TODO(crbug.com/1055150): Determine how this should be best exposed for screen
// readers without over-verbalizing. Currently it reads the full text when
// focused and does not announce when text changes.
void CaptionBubble::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (has_error_) {
    node_data->SetName(error_message_->GetText());
    node_data->SetNameFrom(ax::mojom::NameFrom::kContents);
  } else if (label_->GetText().size()) {
    node_data->SetName(label_->GetText());
    node_data->SetNameFrom(ax::mojom::NameFrom::kContents);
  } else {
    node_data->SetName(title_->GetText());
    node_data->SetNameFrom(ax::mojom::NameFrom::kTitle);
  }
  node_data->SetDescription(title_->GetText());
  node_data->role = ax::mojom::Role::kCaption;
}

void CaptionBubble::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  if (sender == close_button_) {
    DCHECK(GetWidget());
    GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
  }
}

void CaptionBubble::SetText(const std::string& text) {
  label_->SetText(base::ASCIIToUTF16(text));
  UpdateBubbleAndTitleVisibility();
}

void CaptionBubble::SetHasError(bool has_error) {
  if (has_error_ == has_error)
    return;
  has_error_ = has_error;
  label_->SetVisible(!has_error);
  UpdateBubbleAndTitleVisibility();
  error_icon_->SetVisible(has_error);
  error_message_->SetVisible(has_error);
}

void CaptionBubble::UpdateBubbleAndTitleVisibility() {
  // Show the title if there is room for it and no error.
  title_->SetVisible(!has_error_ &&
                     label_->GetPreferredSize().height() <
                         kLineHeightDip * kNumLines * GetTextScaleFactor());
  UpdateBubbleVisibility();
}

void CaptionBubble::UpdateBubbleVisibility() {
  DCHECK(GetWidget());
  // Show the widget if it can be shown, there is room for it and it has text
  // or an error to display.
  if (!should_show_ || !can_layout_) {
    if (GetWidget()->IsVisible())
      GetWidget()->Hide();
  } else if (label_->GetText().size() > 0 || has_error_) {
    // Only show the widget if it isn't already visible. Always calling
    // Widget::Show() will mean the widget gets focus each time.
    if (!GetWidget()->IsVisible())
      GetWidget()->Show();
  } else if (GetWidget()->IsVisible()) {
    // No text and no error. Hide it.
    GetWidget()->Hide();
  }
}

void CaptionBubble::UpdateCaptionStyle(
    base::Optional<ui::CaptionStyle> caption_style) {
  caption_style_ = caption_style;
  UpdateTextSize();
  SizeToContents();
}

void CaptionBubble::Show() {
  should_show_ = true;
  UpdateBubbleVisibility();
}

void CaptionBubble::Hide() {
  should_show_ = false;
  UpdateBubbleVisibility();
}

double CaptionBubble::GetTextScaleFactor() {
  double textScaleFactor = 1;
  if (caption_style_) {
    // ui::CaptionStyle states that text_size is percentage as a CSS string. It
    // can sometimes have !important which is why this is a partial match.
    bool match = RE2::PartialMatch(caption_style_->text_size, "(\\d+)%",
                                   &textScaleFactor);
    textScaleFactor = match ? textScaleFactor / 100 : 1;
  }
  return textScaleFactor;
}

void CaptionBubble::UpdateTextSize() {
  double textScaleFactor = GetTextScaleFactor();

  const gfx::FontList font_list =
      gfx::FontList({kPrimaryFont, kSecondaryFont, kTertiaryFont},
                    gfx::Font::FontStyle::NORMAL, kFontSizePx * textScaleFactor,
                    gfx::Font::Weight::NORMAL);
  label_->SetFontList(font_list);
  title_->SetFontList(font_list);
  error_message_->SetFontList(font_list);

  label_->SetLineHeight(kLineHeightDip * textScaleFactor);
  title_->SetLineHeight(kLineHeightDip * textScaleFactor);
  error_message_->SetLineHeight(kLineHeightDip * textScaleFactor);

  int content_height =
      has_error_ ? kLineHeightDip * textScaleFactor + kErrorImageSizeDip
                 : kLineHeightDip * kNumLines * textScaleFactor;
  content_container_->SetPreferredSize(
      gfx::Size(kMaxWidthDip, content_height + kVerticalMarginsDip));
  // TODO(crbug.com/1055150): On hover, show/hide the close button. At that
  // time remove the height of close button size from SetPreferredSize.
  SetPreferredSize(
      gfx::Size(kMaxWidthDip, content_height + kCloseButtonMargin +
                                  close_button_->GetPreferredSize().height()));
}

}  // namespace captions
