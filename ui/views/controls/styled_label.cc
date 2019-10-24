// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/styled_label.h"

#include <stddef.h>

#include <algorithm>
#include <limits>

#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {

StyledLabel::TestApi::TestApi(StyledLabel* view) : view_(view) {}

StyledLabel::TestApi::~TestApi() = default;

const std::map<View*, gfx::Range>& StyledLabel::TestApi::link_targets() {
  return view_->link_targets_;
}

StyledLabel::RangeStyleInfo::RangeStyleInfo() = default;
StyledLabel::RangeStyleInfo::RangeStyleInfo(const RangeStyleInfo& copy) =
    default;

StyledLabel::RangeStyleInfo::~RangeStyleInfo() = default;

// static
StyledLabel::RangeStyleInfo StyledLabel::RangeStyleInfo::CreateForLink() {
  RangeStyleInfo result;
  result.disable_line_wrapping = true;
  result.text_style = style::STYLE_LINK;
  return result;
}

bool StyledLabel::RangeStyleInfo::IsLink() const {
  return text_style && text_style.value() == style::STYLE_LINK;
}

bool StyledLabel::StyleRange::operator<(
    const StyledLabel::StyleRange& other) const {
  return range.start() < other.range.start();
}

StyledLabel::StyledLabel(const base::string16& text,
                         StyledLabelListener* listener)
    : listener_(listener) {
  base::TrimWhitespace(text, base::TRIM_TRAILING, &text_);
}

StyledLabel::~StyledLabel() = default;

const base::string16& StyledLabel::GetText() const {
  return text_;
}

void StyledLabel::SetText(const base::string16& text) {
  if (text_ == text)
    return;

  text_ = text;
  style_ranges_.clear();
  RemoveAllChildViews(true);
  OnPropertyChanged(&text_, kPropertyEffectsPreferredSizeChanged);
}

gfx::FontList StyledLabel::GetDefaultFontList() const {
  return style::GetFont(text_context_, default_text_style_);
}

void StyledLabel::AddStyleRange(const gfx::Range& range,
                                const RangeStyleInfo& style_info) {
  DCHECK(!range.is_reversed());
  DCHECK(!range.is_empty());
  DCHECK(gfx::Range(0, text_.size()).Contains(range));

  // Insert the new range in sorted order.
  StyleRanges new_range;
  new_range.push_front(StyleRange(range, style_info));
  style_ranges_.merge(new_range);

  PreferredSizeChanged();
}

void StyledLabel::AddCustomView(std::unique_ptr<View> custom_view) {
  DCHECK(custom_view->owned_by_client());
  custom_views_.insert(std::move(custom_view));
}

int StyledLabel::GetTextContext() const {
  return text_context_;
}

void StyledLabel::SetTextContext(int text_context) {
  if (text_context_ == text_context)
    return;

  text_context_ = text_context;
  OnPropertyChanged(&text_context_, kPropertyEffectsPreferredSizeChanged);
}

int StyledLabel::GetDefaultTextStyle() const {
  return default_text_style_;
}

void StyledLabel::SetDefaultTextStyle(int text_style) {
  if (default_text_style_ == text_style)
    return;

  default_text_style_ = text_style;
  OnPropertyChanged(&default_text_style_, kPropertyEffectsPreferredSizeChanged);
}

int StyledLabel::GetLineHeight() const {
  return specified_line_height_;
}

void StyledLabel::SetLineHeight(int line_height) {
  if (specified_line_height_ == line_height)
    return;

  specified_line_height_ = line_height;
  OnPropertyChanged(&specified_line_height_,
                    kPropertyEffectsPreferredSizeChanged);
}

SkColor StyledLabel::GetDisplayedOnBackgroundColor() const {
  return displayed_on_background_color_;
}

void StyledLabel::SetDisplayedOnBackgroundColor(SkColor color) {
  if (displayed_on_background_color_ == color &&
      displayed_on_background_color_set_)
    return;

  displayed_on_background_color_ = color;
  displayed_on_background_color_set_ = true;

  for (View* child : children()) {
    DCHECK((child->GetClassName() == Label::kViewClassName) ||
           (child->GetClassName() == Link::kViewClassName));
    static_cast<Label*>(child)->SetBackgroundColor(color);
  }
  OnPropertyChanged(&displayed_on_background_color_, kPropertyEffectsNone);
}

bool StyledLabel::GetAutoColorReadabilityEnabled() const {
  return auto_color_readability_enabled_;
}

void StyledLabel::SetAutoColorReadabilityEnabled(bool auto_color_readability) {
  if (auto_color_readability_enabled_ == auto_color_readability)
    return;

  auto_color_readability_enabled_ = auto_color_readability;
  OnPropertyChanged(&auto_color_readability_enabled_, kPropertyEffectsNone);
}

void StyledLabel::SizeToFit(int max_width) {
  if (max_width == 0)
    max_width = std::numeric_limits<int>::max();

  SetSize(CalculateAndDoLayout(max_width, true));
}

void StyledLabel::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (text_context_ == style::CONTEXT_DIALOG_TITLE)
    node_data->role = ax::mojom::Role::kTitleBar;
  else
    node_data->role = ax::mojom::Role::kStaticText;

  node_data->SetName(GetText());
}

gfx::Size StyledLabel::CalculatePreferredSize() const {
  // TODO(pkasting): This seems suspicious; shouldn't there be a call to
  // CalculateLayout(std::numeric_limits<int>::max()) here?
  return calculated_size_;
}

int StyledLabel::GetHeightForWidth(int w) const {
  // TODO(erg): Munge the const-ness of the style label. CalculateAndDoLayout
  // doesn't actually make any changes to member variables when |dry_run| is
  // set to true. In general, the mutating and non-mutating parts shouldn't
  // be in the same codepath.
  return const_cast<StyledLabel*>(this)->CalculateAndDoLayout(w, true).height();
}

void StyledLabel::Layout() {
  CalculateAndDoLayout(GetLocalBounds().width(), false);
}

void StyledLabel::PreferredSizeChanged() {
  calculated_size_ = gfx::Size();
  width_at_last_calculation_ = 0;
  width_at_last_layout_ = 0;
  View::PreferredSizeChanged();
}

void StyledLabel::LinkClicked(Link* source, int event_flags) {
  if (listener_)
    listener_->StyledLabelLinkClicked(this, link_targets_[source], event_flags);
}

// TODO(wutao): support gfx::ALIGN_TO_HEAD alignment.
void StyledLabel::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  DCHECK_NE(gfx::ALIGN_TO_HEAD, alignment);
  alignment = gfx::MaybeFlipForRTL(alignment);

  if (horizontal_alignment_ == alignment)
    return;
  horizontal_alignment_ = alignment;
  PreferredSizeChanged();
}

void StyledLabel::ClearStyleRanges() {
  style_ranges_.clear();
  PreferredSizeChanged();
}

int StyledLabel::StartX(int excess_space) const {
  int x = GetInsets().left();
  if (horizontal_alignment_ == gfx::ALIGN_LEFT)
    return x;
  return x + ((horizontal_alignment_ == gfx::ALIGN_CENTER) ? (excess_space / 2)
                                                           : excess_space);
}

int StyledLabel::GetDefaultLineHeight() const {
  return specified_line_height_ > 0
             ? specified_line_height_
             : std::max(
                   style::GetLineHeight(text_context_, default_text_style_),
                   GetDefaultFontList().GetHeight());
}

gfx::FontList StyledLabel::GetFontListForRange(
    const StyleRanges::const_iterator& range) const {
  if (range == style_ranges_.end())
    return GetDefaultFontList();

  return range->style_info.custom_font
             ? range->style_info.custom_font.value()
             : style::GetFont(
                   text_context_,
                   range->style_info.text_style.value_or(default_text_style_));
}

gfx::Size StyledLabel::CalculateAndDoLayout(int width, bool dry_run) {
  if (width == width_at_last_calculation_ &&
      (dry_run || width == width_at_last_layout_))
    return calculated_size_;

  width_at_last_calculation_ = width;
  if (!dry_run)
    width_at_last_layout_ = width;

  const gfx::Insets insets = GetInsets();
  width -= insets.width();

  if (!dry_run) {
    RemoveAllChildViews(true);
    link_targets_.clear();
  }

  const int default_line_height = GetDefaultLineHeight();

  // The current child view's position, relative to content bounds, in pixels.
  gfx::Point offset(0, insets.top());
  int total_height = 0;
  // The max width of all lines. Guaranteed to be no larger than |width|.
  int max_width = 0;

  RangeStyleInfo default_style;
  default_style.text_style = default_text_style_;

  // Try to preserve leading whitespace on the first line.
  bool can_trim_leading_whitespace = false;
  StyleRanges::const_iterator current_range = style_ranges_.begin();
  for (base::string16 remaining_string = text_;
       width > 0 && !remaining_string.empty();) {
    // Max height of the views in a line.
    int line_height = default_line_height;

    // Temporary references to the views in a line, used for alignment.
    std::vector<View*> views_in_a_line;

    while (!remaining_string.empty()) {
      if (offset.x() == 0 && can_trim_leading_whitespace) {
        if (remaining_string.front() == L'\n') {
          // Wrapped to the next line on \n, remove it. Other whitespace,
          // e.g. spaces to indent the next line, are preserved.
          remaining_string.erase(0, 1);
        } else {
          // Wrapped on whitespace character or characters in the middle of the
          // line - none of them are needed at the beginning of the next line.
          base::TrimWhitespace(remaining_string, base::TRIM_LEADING,
                               &remaining_string);
        }
      }

      gfx::Range range = gfx::Range::InvalidRange();
      if (current_range != style_ranges_.end())
        range = current_range->range;

      const size_t position = text_.size() - remaining_string.size();
      std::vector<base::string16> substrings;
      // If the current range is not a custom_view, then we use
      // ElideRectangleText() to determine the line wrapping. Note: if it is a
      // custom_view, then the |position| should equal range.start() because the
      // custom_view is treated as one unit.
      if (position != range.start() ||
          (current_range != style_ranges_.end() &&
           !current_range->style_info.custom_view)) {
        const gfx::Rect chunk_bounds(offset.x(), 0, width - offset.x(),
                                     default_line_height);
        // If the start of the remaining text is inside a styled range, the font
        // style may differ from the base font. The font specified by the range
        // should be used when eliding text.
        gfx::FontList text_font_list = position >= range.start()
                                           ? GetFontListForRange(current_range)
                                           : GetDefaultFontList();
        int elide_result = gfx::ElideRectangleText(
            remaining_string, text_font_list, chunk_bounds.width(),
            chunk_bounds.height(), gfx::WRAP_LONG_WORDS, &substrings);

        if (substrings.empty()) {
          // There is no room for anything. Since wrapping is enabled, this
          // should only occur if there is insufficient vertical space
          // remaining. ElideRectangleText() always adds a single character,
          // even if there is no room horizontally.
          DCHECK_NE(0, elide_result & gfx::INSUFFICIENT_SPACE_VERTICAL);
          // There's no way to continue processing; clear |remaining_string| so
          // the outer loop will terminate after this iteration completes.
          remaining_string.clear();
          break;
        }

        // Views are aligned to integer coordinates, but typesetting is not.
        // This means that it's possible for an ElideRectangleText on a prior
        // iteration to fit a word on the current line, which does not fit after
        // that word is wrapped in a View for its chunk at the end of the line.
        // In most cases, this will just wrap more words on to the next line.
        // However, if the remaining chunk width is insufficient for the very
        // _first_ word, that word will be incorrectly split. In this case,
        // start a new line instead.
        bool truncated_chunk =
            offset.x() != 0 &&
            (elide_result & gfx::INSUFFICIENT_SPACE_FOR_FIRST_WORD) != 0;
        if (substrings[0].empty() || truncated_chunk) {
          // The entire line is \n, or nothing else fits on this line.  Wrap,
          // unless this is the first line, in which case we strip leading
          // whitespace and try again.
          if (offset.x() != 0 || offset.y() > insets.top())
            break;
          DCHECK(views_in_a_line.empty());
          can_trim_leading_whitespace = true;
          continue;
        }
      }

      base::string16 chunk;
      View* custom_view = nullptr;
      std::unique_ptr<Label> label;
      if (position >= range.start()) {
        const RangeStyleInfo& style_info = current_range->style_info;

        if (style_info.custom_view) {
          custom_view = style_info.custom_view;
          // Ownership of the custom view must be passed to StyledLabel.
          DCHECK(std::find_if(custom_views_.cbegin(), custom_views_.cend(),
                              [custom_view](const auto& view) {
                                return view.get() == custom_view;
                              }) != custom_views_.cend());
          // Do not allow wrap in custom view.
          DCHECK_EQ(position, range.start());
          chunk = remaining_string.substr(0, range.end() - position);
        } else {
          chunk = substrings[0];
        }

        if (((custom_view &&
              offset.x() + custom_view->GetPreferredSize().width() > width) ||
             (style_info.disable_line_wrapping &&
              chunk.size() < range.length())) &&
            position == range.start() && offset.x() != 0) {
          // If the chunk should not be wrapped, try to fit it entirely on the
          // next line.
          break;
        }

        if (chunk.size() > range.end() - position)
          chunk = chunk.substr(0, range.end() - position);

        if (!custom_view) {
          label = CreateLabel(chunk, style_info);
          if (style_info.IsLink() && !dry_run) {
            static_cast<Link*>(label.get())->set_listener(this);
            link_targets_[label.get()] = range;
          }
        }

        if (position + chunk.size() >= range.end())
          ++current_range;
      } else {
        chunk = substrings[0];
        if (position + chunk.size() > range.start())
          chunk = chunk.substr(0, range.start() - position);

        // This chunk is normal text.
        label = CreateLabel(chunk, default_style);
      }

      View* child_view = custom_view ? custom_view : label.get();
      gfx::Size size = child_view->GetPreferredSize();
      // A custom view could be wider than the available width.
      size.set_width(std::min(size.width(), width - offset.x()));

      child_view->SetBoundsRect(gfx::Rect(offset, size));
      offset.set_x(offset.x() + size.width());
      line_height = std::max(line_height, size.height());

      if (!dry_run) {
        views_in_a_line.push_back(child_view);
        if (label)
          AddChildView(label.release());
        else
          AddChildView(child_view);
      }

      remaining_string = remaining_string.substr(chunk.size());

      // If |gfx::ElideRectangleText| returned more than one substring, that
      // means the whole text did not fit into remaining line width, with text
      // after |susbtring[0]| spilling into next line. If whole |substring[0]|
      // was added to the current line (this may not be the case if part of the
      // substring has different style), proceed to the next line.
      if (!custom_view && substrings.size() > 1 &&
          chunk.size() == substrings[0].size()) {
        break;
      }
    }
    total_height += line_height;
    max_width = std::max(max_width, offset.x());
    // Trim whitespace at the start of the next line.
    can_trim_leading_whitespace = true;

    // Adjust the positions of the views in the line.
    const int start_x = StartX(width - offset.x());
    for (auto* view : views_in_a_line) {
      view->SetPosition({start_x + view->x(),
                         offset.y() + (line_height - view->height()) / 2.0f});
    }

    // Move to the next line.
    offset = gfx::Point(0, offset.y() + line_height);
  }

  // |width| may be negative if the value passed in was smaller than
  // insets.width().
  DCHECK_LE(max_width, std::max(width, 0));
  calculated_size_ =
      gfx::Size(max_width + insets.width(), total_height + insets.height());
  return calculated_size_;
}

std::unique_ptr<Label> StyledLabel::CreateLabel(
    const base::string16& text,
    const StyledLabel::RangeStyleInfo& style_info) const {
  std::unique_ptr<Label> result;
  if (style_info.IsLink()) {
    // Nothing should (and nothing does) use a custom font for links.
    DCHECK(!style_info.custom_font);

    // Note this ignores |default_text_style_|, in favor of style::STYLE_LINK.
    auto link = std::make_unique<Link>(text, text_context_);

    // Links in a StyledLabel do not get underlines.
    link->SetUnderline(false);

    result = std::move(link);
  } else if (style_info.custom_font) {
    result = std::make_unique<Label>(
        text, Label::CustomFont{style_info.custom_font.value()});
  } else {
    result = std::make_unique<Label>(
        text, text_context_,
        style_info.text_style.value_or(default_text_style_));
  }

  if (style_info.override_color != SK_ColorTRANSPARENT)
    result->SetEnabledColor(style_info.override_color);
  if (!style_info.tooltip.empty())
    result->SetTooltipText(style_info.tooltip);
  if (displayed_on_background_color_set_)
    result->SetBackgroundColor(displayed_on_background_color_);
  result->SetAutoColorReadabilityEnabled(auto_color_readability_enabled_);

  return result;
}

BEGIN_METADATA(StyledLabel)
ADD_PROPERTY_METADATA(StyledLabel, base::string16, Text)
ADD_PROPERTY_METADATA(StyledLabel, int, TextContext)
ADD_PROPERTY_METADATA(StyledLabel, int, DefaultTextStyle)
ADD_PROPERTY_METADATA(StyledLabel, int, LineHeight)
ADD_PROPERTY_METADATA(StyledLabel, bool, AutoColorReadabilityEnabled)
ADD_PROPERTY_METADATA(StyledLabel, SkColor, DisplayedOnBackgroundColor)
METADATA_PARENT_CLASS(View)
END_METADATA()

}  // namespace views
