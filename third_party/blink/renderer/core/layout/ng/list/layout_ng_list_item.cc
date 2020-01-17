// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"

#include "third_party/blink/renderer/core/layout/layout_image_resource_style_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/list_marker_text.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker_image.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

LayoutNGListItem::LayoutNGListItem(Element* element)
    : LayoutNGBlockFlow(element),
      marker_type_(kStatic),
      is_marker_text_updated_(false) {
  SetInline(false);

  SetConsumesSubtreeChangeNotification();
  RegisterSubtreeChangeListenerOnDescendants(true);
}

bool LayoutNGListItem::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGListItem || LayoutNGBlockFlow::IsOfType(type);
}

void LayoutNGListItem::InsertedIntoTree() {
  LayoutNGBlockFlow::InsertedIntoTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutNGListItem::WillBeRemovedFromTree() {
  LayoutNGBlockFlow::WillBeRemovedFromTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutNGListItem::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  LayoutNGBlockFlow::StyleDidChange(diff, old_style);

  if (Marker())
    UpdateMarkerContentIfNeeded();

  if (old_style && (old_style->ListStyleType() != StyleRef().ListStyleType() ||
                    (StyleRef().ListStyleType() == EListStyleType::kString &&
                     old_style->ListStyleStringValue() !=
                         StyleRef().ListStyleStringValue())))
    ListStyleTypeChanged();
}

// If the value of ListStyleType changed, we need to the marker text has been
// updated.
void LayoutNGListItem::ListStyleTypeChanged() {
  if (!is_marker_text_updated_)
    return;

  is_marker_text_updated_ = false;
  if (LayoutObject* marker = Marker()) {
    marker->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kListStyleTypeChange);
  }
}

void LayoutNGListItem::OrdinalValueChanged() {
  if (marker_type_ == kOrdinalValue && is_marker_text_updated_) {
    is_marker_text_updated_ = false;

    // |Marker()| can be a nullptr, for example, in the case of ::after list
    // item elements.
    if (LayoutObject* marker = Marker()) {
      marker->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          layout_invalidation_reason::kListValueChange);
    }
  }
}

void LayoutNGListItem::SubtreeDidChange() {
  LayoutObject* marker = Marker();
  if (!marker)
    return;

  // Make sure an outside marker is a direct child of the list item (not nested
  // inside an anonymous box), and that a marker originated by a ::before or
  // ::after precedes the generated contents.
  if ((!IsInside() && marker->Parent() != this) ||
      (IsPseudoElement() && marker != FirstChild())) {
    marker->Remove();
    AddChild(marker, FirstChild());
  }

  UpdateMarkerContentIfNeeded();
}

void LayoutNGListItem::WillCollectInlines() {
  UpdateMarkerTextIfNeeded();
}

// Returns true if this is 'list-style-position: inside', or should be laid out
// as 'inside'.
bool LayoutNGListItem::IsInside() const {
  return StyleRef().ListStylePosition() == EListStylePosition::kInside ||
         (IsA<HTMLLIElement>(GetNode()) && !StyleRef().IsInsideListElement());
}

void LayoutNGListItem::UpdateMarkerText(LayoutText* text) {
  DCHECK(text);
  StringBuilder marker_text_builder;
  marker_type_ = MarkerText(&marker_text_builder, kWithSuffix);
  text->SetTextIfNeeded(marker_text_builder.ToString().ReleaseImpl());
  is_marker_text_updated_ = true;
}

void LayoutNGListItem::UpdateMarkerText() {
  DCHECK(Marker());
  UpdateMarkerText(ToLayoutText(Marker()->SlowFirstChild()));
}

LayoutNGListItem* LayoutNGListItem::FromMarker(const LayoutObject& marker) {
  DCHECK(marker.IsLayoutNGListMarkerIncludingInside());
  DCHECK(marker.GetNode());
  DCHECK(marker.GetNode()->IsMarkerPseudoElement());
  DCHECK(marker.GetNode()->parentNode());
  DCHECK(marker.GetNode()->parentNode()->GetLayoutObject());
  LayoutObject* parent = marker.GetNode()->parentNode()->GetLayoutObject();
  if (!parent->IsLayoutNGListItem()) {
    // Shouldn't have generated the marker
    NOTREACHED();
    return nullptr;
  }
  return ToLayoutNGListItem(parent);
}

LayoutNGListItem* LayoutNGListItem::FromMarkerOrMarkerContent(
    const LayoutObject& object) {
  DCHECK(object.IsAnonymous());

  if (object.IsLayoutNGListMarkerIncludingInside())
    return FromMarker(object);

  // Check if this is a marker content.
  if (const LayoutObject* parent = object.Parent()) {
    if (parent->IsLayoutNGListMarkerIncludingInside())
      return FromMarker(*parent);
  }

  return nullptr;
}

int LayoutNGListItem::Value() const {
  DCHECK(GetNode());
  return ordinal_.Value(*GetNode());
}

LayoutNGListItem::MarkerType LayoutNGListItem::MarkerText(
    StringBuilder* text,
    MarkerTextFormat format) const {
  if (IsMarkerImage()) {
    if (format == kWithSuffix)
      text->Append(' ');
    return kStatic;
  }

  const ComputedStyle& style = StyleRef();
  switch (style.ListStyleType()) {
    case EListStyleType::kNone:
      return kStatic;
    case EListStyleType::kString: {
      text->Append(style.ListStyleStringValue());
      return kStatic;
    }
    case EListStyleType::kDisc:
    case EListStyleType::kCircle:
    case EListStyleType::kSquare:
      // value is ignored for these types
      text->Append(list_marker_text::GetText(style.ListStyleType(), 0));
      if (format == kWithSuffix)
        text->Append(' ');
      return kSymbolValue;
    case EListStyleType::kArabicIndic:
    case EListStyleType::kArmenian:
    case EListStyleType::kBengali:
    case EListStyleType::kCambodian:
    case EListStyleType::kCjkIdeographic:
    case EListStyleType::kCjkEarthlyBranch:
    case EListStyleType::kCjkHeavenlyStem:
    case EListStyleType::kDecimalLeadingZero:
    case EListStyleType::kDecimal:
    case EListStyleType::kDevanagari:
    case EListStyleType::kEthiopicHalehame:
    case EListStyleType::kEthiopicHalehameAm:
    case EListStyleType::kEthiopicHalehameTiEr:
    case EListStyleType::kEthiopicHalehameTiEt:
    case EListStyleType::kGeorgian:
    case EListStyleType::kGujarati:
    case EListStyleType::kGurmukhi:
    case EListStyleType::kHangul:
    case EListStyleType::kHangulConsonant:
    case EListStyleType::kHebrew:
    case EListStyleType::kHiragana:
    case EListStyleType::kHiraganaIroha:
    case EListStyleType::kKannada:
    case EListStyleType::kKatakana:
    case EListStyleType::kKatakanaIroha:
    case EListStyleType::kKhmer:
    case EListStyleType::kKoreanHangulFormal:
    case EListStyleType::kKoreanHanjaFormal:
    case EListStyleType::kKoreanHanjaInformal:
    case EListStyleType::kLao:
    case EListStyleType::kLowerAlpha:
    case EListStyleType::kLowerArmenian:
    case EListStyleType::kLowerGreek:
    case EListStyleType::kLowerLatin:
    case EListStyleType::kLowerRoman:
    case EListStyleType::kMalayalam:
    case EListStyleType::kMongolian:
    case EListStyleType::kMyanmar:
    case EListStyleType::kOriya:
    case EListStyleType::kPersian:
    case EListStyleType::kSimpChineseFormal:
    case EListStyleType::kSimpChineseInformal:
    case EListStyleType::kTelugu:
    case EListStyleType::kThai:
    case EListStyleType::kTibetan:
    case EListStyleType::kTradChineseFormal:
    case EListStyleType::kTradChineseInformal:
    case EListStyleType::kUpperAlpha:
    case EListStyleType::kUpperArmenian:
    case EListStyleType::kUpperLatin:
    case EListStyleType::kUpperRoman:
    case EListStyleType::kUrdu: {
      int value = Value();
      text->Append(list_marker_text::GetText(style.ListStyleType(), value));
      if (format == kWithSuffix) {
        text->Append(list_marker_text::Suffix(style.ListStyleType(), value));
        text->Append(' ');
      }
      return kOrdinalValue;
    }
  }
  NOTREACHED();
  return kStatic;
}

String LayoutNGListItem::MarkerTextWithSuffix() const {
  StringBuilder text;
  MarkerText(&text, kWithSuffix);
  return text.ToString();
}

String LayoutNGListItem::MarkerTextWithoutSuffix() const {
  StringBuilder text;
  MarkerText(&text, kWithoutSuffix);
  return text.ToString();
}

String LayoutNGListItem::TextAlternative(const LayoutObject& marker) {
  // For accessibility, return the marker string in the logical order even in
  // RTL, reflecting speech order.
  if (LayoutNGListItem* list_item = FromMarker(marker))
    return list_item->MarkerTextWithSuffix();
  return g_empty_string;
}

void LayoutNGListItem::UpdateMarkerContentIfNeeded() {
  LayoutObject* marker = Marker();
  DCHECK(marker);

  LayoutObject* child = marker->SlowFirstChild();
  // There should be at most one child.
  DCHECK(!child || !child->SlowFirstChild());
  if (!marker->StyleRef().ContentBehavesAsNormal()) {
    marker_type_ = kStatic;
    is_marker_text_updated_ = true;
  } else if (IsMarkerImage()) {
    StyleImage* list_style_image = StyleRef().ListStyleImage();
    if (child) {
      // If the url of `list-style-image` changed, create a new LayoutImage.
      if (!child->IsLayoutImage() ||
          ToLayoutImage(child)->ImageResource()->ImagePtr() !=
              list_style_image->Data()) {
        child->Destroy();
        child = nullptr;
      }
    }
    if (!child) {
      LayoutNGListMarkerImage* image =
          LayoutNGListMarkerImage::CreateAnonymous(&GetDocument());
      scoped_refptr<ComputedStyle> image_style =
          ComputedStyle::CreateAnonymousStyleWithDisplay(marker->StyleRef(),
                                                         EDisplay::kInline);
      image->SetStyle(image_style);
      image->SetImageResource(
          MakeGarbageCollected<LayoutImageResourceStyleImage>(
              list_style_image));
      image->SetIsGeneratedContent();
      marker->AddChild(image);
    }
  } else if (StyleRef().ListStyleType() == EListStyleType::kNone) {
    marker_type_ = kStatic;
    is_marker_text_updated_ = true;
  } else {
    // Create a LayoutText in it.
    LayoutText* text = nullptr;
    // |text_style| should be as same as style propagated in
    // |LayoutObject::PropagateStyleToAnonymousChildren()| to avoid unexpected
    // full layout due by style difference. See http://crbug.com/980399
    scoped_refptr<ComputedStyle> text_style =
        ComputedStyle::CreateAnonymousStyleWithDisplay(
            marker->StyleRef(), marker->StyleRef().Display());
    if (child) {
      if (child->IsText()) {
        text = ToLayoutText(child);
        text->SetStyle(text_style);
      } else {
        child->Destroy();
        child = nullptr;
      }
    }
    if (!child) {
      text = LayoutText::CreateEmptyAnonymous(GetDocument(), text_style,
                                              LegacyLayout::kAuto);
      marker->AddChild(text);
      is_marker_text_updated_ = false;
    }
  }
}

LayoutObject* LayoutNGListItem::SymbolMarkerLayoutText() const {
  if (marker_type_ != kSymbolValue)
    return nullptr;
  DCHECK(Marker());
  return Marker()->SlowFirstChild();
}

const LayoutObject* LayoutNGListItem::FindSymbolMarkerLayoutText(
    const LayoutObject* object) {
  if (!object)
    return nullptr;

  if (object->IsLayoutNGListItem())
    return ToLayoutNGListItem(object)->SymbolMarkerLayoutText();

  if (object->IsLayoutNGListMarker())
    return ToLayoutNGListMarker(object)->SymbolMarkerLayoutText();

  if (object->IsAnonymousBlock())
    return FindSymbolMarkerLayoutText(object->Parent());

  return nullptr;
}

}  // namespace blink
