// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "extensions/common/image_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace {

// The left-hand margin used for rows.
static constexpr int kMarginLeft = 4;

// TODO(dschuyler): Perhaps this should be based on the font size
// instead of hardcoded to 2 dp (e.g. by adding a space in an
// appropriate font to the beginning of the description, then reducing
// the additional padding here to zero).
static constexpr int kAnswerIconToTextPadding = 2;

// The edge length of the layout image area.
static constexpr int kImageBoxSize = 40;

// The diameter of the new answer layout images.
static constexpr int kNewAnswerImageSize = 24;

// The edge length of the entity suggestions images.
static constexpr int kEntityImageSize = 32;
static constexpr int kEntityImageCornerRadius = 4;

// The margin height of a one-line suggestion row.
static constexpr int kOneLineRowMarginHeight = 8;

// The margin height of a two-line suggestion row.
static constexpr int kTwoLineRowMarginHeight = 4;

// Returns the padding width between elements.
int HorizontalPadding() {
  return GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) +
         GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).width() / 2;
}

// Returns the margins that should appear around the result.
// |is_two_line| indicates whether the vertical margin is for a omnibox
// result displaying an answer to the query.
gfx::Insets GetMarginInsets(int text_height, bool is_two_line) {
  int vertical_margin =
      is_two_line ? kTwoLineRowMarginHeight : kOneLineRowMarginHeight;

  base::Optional<int> vertical_margin_override =
      OmniboxFieldTrial::GetSuggestionVerticalMarginFieldTrialOverride();
  if (vertical_margin_override) {
    // If the vertical margin experiment is on, we purposely set both the
    // one-line and two-line suggestions to have the same vertical margin.
    //
    // There is no vertical margin value we could set to make the new answer
    // style look anything similar to the pre-Refresh style, but setting them to
    // be the same looks reasonable, and is a sane place to start experimenting.
    vertical_margin = vertical_margin_override.value();
  }

  return gfx::Insets(vertical_margin, kMarginLeft, vertical_margin,
                     OmniboxMatchCellView::kMarginRight);
}

////////////////////////////////////////////////////////////////////////////////
// PlaceholderImageSource:

class PlaceholderImageSource : public gfx::CanvasImageSource {
 public:
  PlaceholderImageSource(const gfx::Size& canvas_size, SkColor color);
  ~PlaceholderImageSource() override;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

 private:
  SkColor color_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(PlaceholderImageSource);
};

PlaceholderImageSource::PlaceholderImageSource(const gfx::Size& canvas_size,
                                               SkColor color)
    : gfx::CanvasImageSource(canvas_size), color_(color), size_(canvas_size) {}

PlaceholderImageSource::~PlaceholderImageSource() = default;

void PlaceholderImageSource::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStrokeAndFill_Style);
  flags.setColor(color_);
  canvas->sk_canvas()->drawRoundRect(gfx::RectToSkRect(gfx::Rect(size_)),
                                     kEntityImageCornerRadius,
                                     kEntityImageCornerRadius, flags);
}

////////////////////////////////////////////////////////////////////////////////
// EncircledImageSource:

class EncircledImageSource : public gfx::CanvasImageSource {
 public:
  EncircledImageSource(const int radius,
                       const SkColor color,
                       const gfx::ImageSkia& image);
  ~EncircledImageSource() override;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

 private:
  const int radius_;
  const SkColor color_;
  const gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(EncircledImageSource);
};

EncircledImageSource::EncircledImageSource(const int radius,
                                           const SkColor color,
                                           const gfx::ImageSkia& image)
    : gfx::CanvasImageSource(gfx::Size(radius * 2, radius * 2)),
      radius_(radius),
      color_(color),
      image_(image) {}

EncircledImageSource::~EncircledImageSource() = default;

void EncircledImageSource::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStrokeAndFill_Style);
  flags.setColor(color_);
  canvas->DrawCircle(gfx::Point(radius_, radius_), radius_, flags);
  const int x = radius_ - image_.width() / 2;
  const int y = radius_ - image_.height() / 2;
  canvas->DrawImageInt(image_, x, y);
}

////////////////////////////////////////////////////////////////////////////////
// RoundedCornerImageView:

class RoundedCornerImageView : public views::ImageView {
 public:
  RoundedCornerImageView() = default;

  bool CanProcessEventsWithinSubtree() const override { return false; }

 private:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;

  DISALLOW_COPY_AND_ASSIGN(RoundedCornerImageView);
};

void RoundedCornerImageView::OnPaint(gfx::Canvas* canvas) {
  SkPath mask;
  mask.addRoundRect(gfx::RectToSkRect(GetImageBounds()),
                    kEntityImageCornerRadius, kEntityImageCornerRadius);
  canvas->ClipPath(mask, true);
  ImageView::OnPaint(canvas);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxMatchCellView:

OmniboxMatchCellView::OmniboxMatchCellView(OmniboxResultView* result_view) {
  AddChildView(icon_view_ = new views::ImageView());
  AddChildView(answer_image_view_ = new RoundedCornerImageView());
  AddChildView(content_view_ = new OmniboxTextView(result_view));
  AddChildView(description_view_ = new OmniboxTextView(result_view));
  AddChildView(separator_view_ = new OmniboxTextView(result_view));

  using Alignment = views::ImageView::Alignment;
  icon_view_->SetHorizontalAlignment(Alignment::kCenter);
  icon_view_->SetVerticalAlignment(Alignment::kCenter);
  answer_image_view_->SetHorizontalAlignment(Alignment::kCenter);
  answer_image_view_->SetVerticalAlignment(Alignment::kCenter);

  const base::string16 separator = l10n_util::GetStringUTF16(
      base::FeatureList::IsEnabled(
          omnibox::kOmniboxAlternateMatchDescriptionSeparator)
          ? IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR_ALTERNATE
          : IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
  separator_view_->SetText(separator);
}

OmniboxMatchCellView::~OmniboxMatchCellView() = default;

gfx::Size OmniboxMatchCellView::CalculatePreferredSize() const {
  int height = 0;
  switch (layout_style_) {
    case LayoutStyle::OLD_ANSWER: {
      int icon_width = icon_view_->width();
      int answer_image_size =
          answer_image_view_->GetImage().isNull()
              ? 0
              : answer_image_view_->height() + kAnswerIconToTextPadding;
      int deduction =
          icon_width + (HorizontalPadding() * 3) + answer_image_size;
      int description_width = std::max(width() - deduction, 0);
      height = content_view_->GetLineHeight() +
               description_view_->GetHeightForWidth(description_width);
      break;
    }
    case LayoutStyle::ONE_LINE_SUGGESTION: {
      height = content_view_->GetLineHeight();
      break;
    }
    case LayoutStyle::TWO_LINE_SUGGESTION: {
      height = content_view_->GetLineHeight() +
               description_view_->GetHeightForWidth(width() - GetTextIndent());
      break;
    }
  }
  height += GetInsets().height();
  // Width is not calculated because it's not needed by current callers.
  return gfx::Size(0, height);
}

bool OmniboxMatchCellView::CanProcessEventsWithinSubtree() const {
  return false;
}

// static
int OmniboxMatchCellView::GetTextIndent() {
  return ui::MaterialDesignController::touch_ui() ? 51 : 47;
}

void OmniboxMatchCellView::OnMatchUpdate(const OmniboxResultView* result_view,
                                         const AutocompleteMatch& match) {
  is_rich_suggestion_ = !!match.answer ||
                        match.type == AutocompleteMatchType::CALCULATOR ||
                        !match.image_url.empty();
  is_search_type_ = AutocompleteMatch::IsSearchType(match.type);

  // Decide layout style once before Layout, while match data is available.
  if (is_rich_suggestion_ || match.ShouldShowTabMatch() || match.pedal) {
    layout_style_ = LayoutStyle::TWO_LINE_SUGGESTION;
  } else if (!!match.answer) {
    layout_style_ = LayoutStyle::OLD_ANSWER;
  } else {
    layout_style_ = LayoutStyle::ONE_LINE_SUGGESTION;
  }

  // Set up the separator.
  separator_view_->SetSize(layout_style_ == LayoutStyle::ONE_LINE_SUGGESTION
                               ? separator_view_->GetPreferredSize()
                               : gfx::Size());

  // Set up the small icon.
  if (is_rich_suggestion_) {
    icon_view_->SetSize(gfx::Size());
  } else {
    icon_view_->SetSize(icon_view_->CalculatePreferredSize());
  }

  const auto apply_vector_icon = [=](const gfx::VectorIcon& vector_icon) {
    const auto& icon = gfx::CreateVectorIcon(vector_icon, SK_ColorWHITE);
    answer_image_view_->SetImage(
        gfx::CanvasImageSource::MakeImageSkia<EncircledImageSource>(
            kNewAnswerImageSize / 2, gfx::kGoogleBlue600, icon));
  };
  if (match.type == AutocompleteMatchType::CALCULATOR) {
    apply_vector_icon(omnibox::kAnswerCalculatorIcon);
  } else if (!is_rich_suggestion_) {
    // An old style answer entry may use the answer_image_view_. But
    // it's set when the image arrives (later).
    answer_image_view_->SetImage(gfx::ImageSkia());
    answer_image_view_->SetSize(gfx::Size());
  } else {
    // Determine if we have a local icon (or else it will be downloaded).
    if (match.answer) {
      switch (match.answer->type()) {
        case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
          apply_vector_icon(omnibox::kAnswerCurrencyIcon);
          break;
        case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
          apply_vector_icon(omnibox::kAnswerDictionaryIcon);
          break;
        case SuggestionAnswer::ANSWER_TYPE_FINANCE:
          apply_vector_icon(omnibox::kAnswerFinanceIcon);
          break;
        case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
          apply_vector_icon(omnibox::kAnswerSunriseIcon);
          break;
        case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
          apply_vector_icon(omnibox::kAnswerTranslationIcon);
          break;
        case SuggestionAnswer::ANSWER_TYPE_WEATHER:
          // Weather icons are downloaded. Do nothing.
          break;
        case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
          apply_vector_icon(omnibox::kAnswerWhenIsIcon);
          break;
        default:
          apply_vector_icon(omnibox::kAnswerDefaultIcon);
          break;
      }
      // Always set the image size so that downloaded images get the correct
      // size (such as Weather answers).
      answer_image_view_->SetImageSize(
          gfx::Size(kNewAnswerImageSize, kNewAnswerImageSize));
    } else {
      SkColor color = result_view->GetColor(OmniboxPart::RESULTS_BACKGROUND);
      extensions::image_util::ParseHexColorString(match.image_dominant_color,
                                                  &color);
      color = SkColorSetA(color, 0x40);  // 25% transparency (arbitrary).
      const gfx::Size size = gfx::Size(kEntityImageSize, kEntityImageSize);
      answer_image_view_->SetImageSize(size);
      answer_image_view_->SetImage(
          gfx::CanvasImageSource::MakeImageSkia<PlaceholderImageSource>(size,
                                                                        color));
    }
  }
  if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL)
    // Used for indent calculation.
    SetTailSuggestCommonPrefixWidth(match.tail_suggest_common_prefix);
  else
    SetTailSuggestCommonPrefixWidth(base::string16());
}

void OmniboxMatchCellView::SetImage(const gfx::ImageSkia& image) {
  answer_image_view_->SetImage(image);

  // Usually, answer images are square. But if that's not the case, setting
  // answer_image_view_ size proportional to the image size preserves
  // the aspect ratio.
  int width = image.width();
  int height = image.height();
  if (width == height)
    return;
  int max = std::max(width, height);
  width = kEntityImageSize * width / max;
  height = kEntityImageSize * height / max;
  answer_image_view_->SetImageSize(gfx::Size(width, height));
}

const char* OmniboxMatchCellView::GetClassName() const {
  return "OmniboxMatchCellView";
}

void OmniboxMatchCellView::Layout() {
  // Update the margins.
  gfx::Insets insets =
      GetMarginInsets(content()->GetLineHeight(),
                      layout_style_ != LayoutStyle::ONE_LINE_SUGGESTION);
  SetBorder(views::CreateEmptyBorder(insets.top(), insets.left(),
                                     insets.bottom(), insets.right()));
  // Layout children *after* updating the margins.
  views::View::Layout();

  const int icon_view_width = kImageBoxSize;
  const int text_indent = GetTextIndent();
  switch (layout_style_) {
    case LayoutStyle::OLD_ANSWER:
      LayoutOldStyleAnswer(icon_view_width, text_indent);
      break;
    case LayoutStyle::ONE_LINE_SUGGESTION:
      LayoutOneLineSuggestion(icon_view_width,
                              text_indent + tail_suggest_common_prefix_width_);
      break;
    case LayoutStyle::TWO_LINE_SUGGESTION:
      LayoutNewStyleTwoLineSuggestion();
      break;
  }
}

void OmniboxMatchCellView::LayoutOldStyleAnswer(int icon_view_width,
                                                int text_indent) {
  // TODO(dschuyler): Remove this layout once rich layouts are enabled by
  // default.
  gfx::Rect child_area = GetContentsBounds();
  const int text_height = content_view_->GetLineHeight();
  int x = child_area.x();
  int y = child_area.y();
  icon_view_->SetBounds(x, y, icon_view_width, text_height);
  x += text_indent;
  content_view_->SetBounds(x, y, width() - x, text_height);
  y += text_height;
  if (!answer_image_view_->GetImage().isNull()) {
    constexpr int kImageEdgeLength = 24;
    constexpr int kImagePadding = 2;
    answer_image_view_->SetBounds(x, y + kImagePadding, kImageEdgeLength,
                                  kImageEdgeLength);
    answer_image_view_->SetImageSize(
        gfx::Size(kImageEdgeLength, kImageEdgeLength));
    x += answer_image_view_->width() + kAnswerIconToTextPadding;
  }
  int description_width = width() - x;
  description_view_->SetBounds(
      x, y, description_width,
      description_view_->GetHeightForWidth(description_width));
}

void OmniboxMatchCellView::LayoutNewStyleTwoLineSuggestion() {
  gfx::Rect child_area = GetContentsBounds();
  int x = child_area.x();
  int y = child_area.y();
  views::ImageView* image_view;
  if (is_rich_suggestion_) {
    image_view = answer_image_view_;
  } else {
    image_view = icon_view_;
  }
  image_view->SetBounds(x, y, kImageBoxSize, child_area.height());
  const int text_width = child_area.width() - GetTextIndent();
  if (description_view_->text().empty()) {
    // This vertically centers content in the rare case that no description is
    // provided.
    content_view_->SetBounds(
        x + GetTextIndent() + tail_suggest_common_prefix_width_, y, text_width,
        child_area.height());
    description_view_->SetSize(gfx::Size());
  } else {
    const int text_height = content_view_->GetLineHeight();
    content_view_->SetBounds(x + GetTextIndent(), y, text_width, text_height);
    description_view_->SetBounds(
        x + GetTextIndent(), y + text_height, text_width,
        description_view_->GetHeightForWidth(text_width));
  }
}

void OmniboxMatchCellView::LayoutOneLineSuggestion(int icon_view_width,
                                                   int text_indent) {
  gfx::Rect child_area = GetContentsBounds();
  int row_height = child_area.height();
  int y = child_area.y();
  icon_view_->SetBounds(child_area.x(), y, icon_view_width, row_height);
  int content_width = content_view_->GetPreferredSize().width();
  int description_width = description_view_->GetPreferredSize().width();
  gfx::Size separator_size = separator_view_->GetPreferredSize();
  OmniboxPopupModel::ComputeMatchMaxWidths(
      content_width, separator_size.width(), description_width,
      child_area.width() - text_indent,
      /*description_on_separate_line=*/false, !is_search_type_, &content_width,
      &description_width);
  int x = child_area.x() + text_indent;
  content_view_->SetBounds(x, y, content_width, row_height);
  if (description_width != 0) {
    x += content_view_->width();
    separator_view_->SetSize(separator_size);
    separator_view_->SetBounds(x, y, separator_view_->width(), row_height);
    x += separator_view_->width();
    description_view_->SetBounds(x, y, description_width, row_height);
  } else {
    description_view_->SetSize(gfx::Size());
    separator_view_->SetSize(gfx::Size());
  }
}

void OmniboxMatchCellView::SetTailSuggestCommonPrefixWidth(
    const base::string16& common_prefix) {
  if (common_prefix.empty()) {
    tail_suggest_common_prefix_width_ = 0;
    return;
  }
  std::unique_ptr<gfx::RenderText> render_text =
      content_view_->CreateRenderText(common_prefix);
  auto size = render_text->GetStringSize();
  tail_suggest_common_prefix_width_ = size.width();
  // Only calculate fixed string width once.
  if (!ellipsis_width_) {
    render_text->SetText(base::ASCIIToUTF16(AutocompleteMatch::kEllipsis));
    size = render_text->GetStringSize();
    ellipsis_width_ = size.width();
  }
  // Indent text by prefix, but come back by width of ellipsis.
  tail_suggest_common_prefix_width_ -= ellipsis_width_;
}
