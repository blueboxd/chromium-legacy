// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGFragmentItem::NGFragmentItem(const NGPhysicalTextFragment& text)
    : layout_object_(text.GetLayoutObject()),
      text_({text.TextShapeResult(), text.StartOffset(), text.EndOffset()}),
      rect_({PhysicalOffset(), text.Size()}),
      type_(kText),
      sub_type_(static_cast<unsigned>(text.TextType())),
      style_variant_(static_cast<unsigned>(text.StyleVariant())),
      is_generated_text_(text.IsGeneratedText()),
      is_hidden_for_paint_(text.IsHiddenForPaint()),
      text_direction_(static_cast<unsigned>(text.ResolvedDirection())) {
  DCHECK_LE(text_.start_offset, text_.end_offset);
#if DCHECK_IS_ON()
  if (text_.shape_result) {
    DCHECK_EQ(text_.shape_result->StartIndex(), text_.start_offset);
    DCHECK_EQ(text_.shape_result->EndIndex(), text_.end_offset);
  }
#endif
}

NGFragmentItem::NGFragmentItem(const NGPhysicalLineBoxFragment& line,
                               wtf_size_t item_count)
    : layout_object_(line.ContainerLayoutObject()),
      line_({&line, item_count}),
      rect_({PhysicalOffset(), line.Size()}),
      type_(kLine),
      style_variant_(static_cast<unsigned>(line.StyleVariant())),
      is_hidden_for_paint_(false),
      text_direction_(static_cast<unsigned>(line.BaseDirection())) {}

NGFragmentItem::NGFragmentItem(const NGPhysicalBoxFragment& box,
                               wtf_size_t item_count,
                               TextDirection resolved_direction)
    : layout_object_(box.GetLayoutObject()),
      box_({&box, item_count}),
      rect_({PhysicalOffset(), box.Size()}),
      type_(kBox),
      style_variant_(static_cast<unsigned>(box.StyleVariant())),
      is_hidden_for_paint_(box.IsHiddenForPaint()),
      text_direction_(static_cast<unsigned>(resolved_direction)) {}

NGFragmentItem::~NGFragmentItem() {
  switch (Type()) {
    case kText:
      text_.~TextItem();
      break;
    case kGeneratedText:
      generated_text_.~GeneratedTextItem();
      break;
    case kLine:
      line_.~LineItem();
      break;
    case kBox:
      box_.~BoxItem();
      break;
  }
}

bool NGFragmentItem::HasSameParent(const NGFragmentItem& other) const {
  if (!GetLayoutObject())
    return !other.GetLayoutObject();
  if (!other.GetLayoutObject())
    return false;
  return GetLayoutObject()->Parent() == other.GetLayoutObject()->Parent();
}

bool NGFragmentItem::IsAtomicInline() const {
  if (Type() != kBox)
    return false;
  if (const NGPhysicalBoxFragment* box = BoxFragment())
    return box->IsAtomicInline();
  return false;
}

bool NGFragmentItem::IsEmptyLineBox() const {
  // TODO(yosin): Implement |NGFragmentItem::IsEmptyLineBox()|.
  return false;
}

bool NGFragmentItem::IsGeneratedText() const {
  if (Type() == kText || Type() == kGeneratedText)
    return is_generated_text_;
  NOTREACHED();
  return false;
}

bool NGFragmentItem::IsListMarker() const {
  // TODO(yosin): Implement |NGFragmentItem::IsListMarker()|.
  return false;
}

PhysicalRect NGFragmentItem::SelfInkOverflow() const {
  // TODO(kojii): Implement.
  return LocalRect();
}

const ShapeResultView* NGFragmentItem::TextShapeResult() const {
  if (Type() == kText)
    return text_.shape_result.get();
  if (Type() == kGeneratedText)
    return generated_text_.shape_result.get();
  NOTREACHED();
  return nullptr;
}

unsigned NGFragmentItem::StartOffset() const {
  if (Type() == kText)
    return text_.start_offset;
  if (Type() == kGeneratedText)
    return 0;
  NOTREACHED();
  return 0;
}

unsigned NGFragmentItem::EndOffset() const {
  if (Type() == kText)
    return text_.end_offset;
  if (Type() == kGeneratedText)
    return generated_text_.text.length();
  NOTREACHED();
  return 0;
}

StringView NGFragmentItem::Text(const NGFragmentItems& items) const {
  if (Type() == kText) {
    DCHECK_LE(text_.start_offset, text_.end_offset);
    return StringView(items.Text(UsesFirstLineStyle()), text_.start_offset,
                      text_.end_offset - text_.start_offset);
  }
  NOTREACHED();
  return StringView();
}

NGTextFragmentPaintInfo NGFragmentItem::TextPaintInfo(
    const NGFragmentItems& items) const {
  if (Type() == kText) {
    return {items.Text(UsesFirstLineStyle()), text_.start_offset,
            text_.end_offset, text_.shape_result.get()};
  }
  NOTREACHED();
  return {};
}

TextDirection NGFragmentItem::BaseDirection() const {
  DCHECK_EQ(Type(), kLine);
  return static_cast<TextDirection>(text_direction_);
}

TextDirection NGFragmentItem::ResolvedDirection() const {
  DCHECK(Type() == kText || Type() == kGeneratedText || IsAtomicInline());
  return static_cast<TextDirection>(text_direction_);
}

String NGFragmentItem::DebugName() const {
  return "NGFragmentItem";
}

IntRect NGFragmentItem::VisualRect() const {
  // TODO(kojii): Need to reconsider the storage of |VisualRect|, to integrate
  // better with |FragmentData| and to avoid dependency to |LayoutObject|.
  DCHECK(GetLayoutObject());
  return GetLayoutObject()->VisualRectForInlineBox();
}

PhysicalRect NGFragmentItem::LocalVisualRectFor(
    const LayoutObject& layout_object) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());

  PhysicalRect visual_rect;
  for (const NGFragmentItem& item : ItemsFor(layout_object)) {
    if (UNLIKELY(item.IsHiddenForPaint()))
      continue;
    PhysicalRect child_visual_rect = item.SelfInkOverflow();
    child_visual_rect.offset += item.Offset();
    visual_rect.Unite(child_visual_rect);
  }
  return visual_rect;
}

NGFragmentItem::ItemsForLayoutObject NGFragmentItem::ItemsFor(
    const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());
  DCHECK(layout_object.IsText() || layout_object.IsLayoutInline() ||
         (layout_object.IsBox() && layout_object.IsInline()));

  // TODO(kojii): This is a hot function needed by paint and several other
  // operations. Make this fast, by not iterating.
  if (const LayoutBlockFlow* block_flow =
          layout_object.RootInlineFormattingContext()) {
    if (const NGPhysicalBoxFragment* fragment = block_flow->CurrentFragment()) {
      if (const NGFragmentItems* items = fragment->Items()) {
        for (unsigned i = 0; i < items->Items().size(); ++i) {
          const NGFragmentItem* item = items->Items()[i].get();
          if (item->GetLayoutObject() == &layout_object)
            return ItemsForLayoutObject(items->Items(), i, item);
        }
      }
    }
  }

  return ItemsForLayoutObject();
}

NGFragmentItem::ItemsForLayoutObject::Iterator&
NGFragmentItem::ItemsForLayoutObject::Iterator::operator++() {
  // TODO(kojii): This is a hot function needed by paint and several other
  // operations. Make this fast, by not iterating.
  if (!current_)
    return *this;
  const LayoutObject* current_layout_object = current_->GetLayoutObject();
  while (++index_ < items_->size()) {
    current_ = (*items_)[index_].get();
    if (current_->GetLayoutObject() == current_layout_object)
      return *this;
  }
  current_ = nullptr;
  return *this;
}

std::ostream& operator<<(std::ostream& ostream, const NGFragmentItem& item) {
  ostream << "{";
  switch (item.Type()) {
    case NGFragmentItem::kText:
      ostream << "Text " << item.StartOffset() << "-" << item.EndOffset() << " "
              << (IsLtr(item.ResolvedDirection()) ? "LTR" : "RTL");
      break;
    case NGFragmentItem::kGeneratedText:
      ostream << "GeneratedText \"" << item.GeneratedText() << "\"";
      break;
    case NGFragmentItem::kLine:
      ostream << "Line #descendants=" << item.DescendantsCount() << " "
              << (IsLtr(item.BaseDirection()) ? "LTR" : "RTL");
      break;
    case NGFragmentItem::kBox:
      ostream << "Box #descendants=" << item.DescendantsCount();
      if (item.IsAtomicInline()) {
        ostream << " AtomicInline"
                << (IsLtr(item.ResolvedDirection()) ? "LTR" : "RTL");
      }
      break;
  }
  ostream << " ";
  switch (item.StyleVariant()) {
    case NGStyleVariant::kStandard:
      ostream << "Standard";
      break;
    case NGStyleVariant::kFirstLine:
      ostream << "FirstLine";
      break;
    case NGStyleVariant::kEllipsis:
      ostream << "Ellipsis";
      break;
  }
  return ostream << "}";
}

std::ostream& operator<<(std::ostream& ostream, const NGFragmentItem* item) {
  if (!item)
    return ostream << "<null>";
  return ostream << *item;
}

}  // namespace blink
