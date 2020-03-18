// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/list_marker.h"

#include "third_party/blink/renderer/core/layout/layout_image_resource_style_image.h"
#include "third_party/blink/renderer/core/layout/list_marker_text.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker_image.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_outside_list_marker.h"

namespace blink {

ListMarker::ListMarker() : marker_text_type_(kNotText) {}

const ListMarker* ListMarker::Get(const LayoutObject* object) {
  if (!object)
    return nullptr;
  if (object->IsLayoutNGOutsideListMarker())
    return &ToLayoutNGOutsideListMarker(object)->Marker();
  if (object->IsLayoutNGInsideListMarker())
    return &ToLayoutNGInsideListMarker(object)->Marker();
  return nullptr;
}

ListMarker* ListMarker::Get(LayoutObject* object) {
  return const_cast<ListMarker*>(
      ListMarker::Get(static_cast<const LayoutObject*>(object)));
}

// If the value of ListStyleType changed, we need to the marker text has been
// updated.
void ListMarker::ListStyleTypeChanged(LayoutObject& marker) {
  if (marker_text_type_ == kNotText || marker_text_type_ == kUnresolved)
    return;

  marker_text_type_ = kUnresolved;
  marker.SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kListStyleTypeChange);
}

void ListMarker::OrdinalValueChanged(LayoutObject& marker) {
  if (marker_text_type_ == kOrdinalValue) {
    marker_text_type_ = kUnresolved;
    marker.SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kListValueChange);
  }
}

void ListMarker::UpdateMarkerText(LayoutObject& marker, LayoutText* text) {
  DCHECK(text);
  DCHECK_EQ(marker_text_type_, kUnresolved);
  StringBuilder marker_text_builder;
  marker_text_type_ = MarkerText(marker, &marker_text_builder, kWithSuffix);
  text->SetTextIfNeeded(marker_text_builder.ToString().ReleaseImpl());
  DCHECK_NE(marker_text_type_, kNotText);
  DCHECK_NE(marker_text_type_, kUnresolved);
}

void ListMarker::UpdateMarkerText(LayoutObject& marker) {
  UpdateMarkerText(marker, ToLayoutText(marker.SlowFirstChild()));
}

LayoutNGListItem* ListMarker::ListItem(const LayoutObject& marker) {
  return ToLayoutNGListItem(marker.GetNode()->parentNode()->GetLayoutObject());
}

ListMarker::MarkerTextType ListMarker::MarkerText(
    const LayoutObject& marker,
    StringBuilder* text,
    MarkerTextFormat format) const {
  if (IsMarkerImage(marker)) {
    if (format == kWithSuffix)
      text->Append(' ');
    return kNotText;
  }

  LayoutNGListItem* list_item = ListItem(marker);
  const ComputedStyle& style = list_item->StyleRef();
  switch (style.ListStyleType()) {
    case EListStyleType::kNone:
      return kNotText;
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
      int value = list_item->Value();
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

String ListMarker::MarkerTextWithSuffix(const LayoutObject& marker) const {
  StringBuilder text;
  MarkerText(marker, &text, kWithSuffix);
  return text.ToString();
}

String ListMarker::MarkerTextWithoutSuffix(const LayoutObject& marker) const {
  StringBuilder text;
  MarkerText(marker, &text, kWithoutSuffix);
  return text.ToString();
}

String ListMarker::TextAlternative(const LayoutObject& marker) const {
  // For accessibility, return the marker string in the logical order even in
  // RTL, reflecting speech order.
  return MarkerTextWithSuffix(marker);
}

void ListMarker::UpdateMarkerContentIfNeeded(LayoutObject& marker) {
  LayoutNGListItem* list_item = ListItem(marker);

  if (!marker.StyleRef().ContentBehavesAsNormal()) {
    marker_text_type_ = kNotText;
    return;
  }

  // There should be at most one child.
  LayoutObject* child = marker.SlowFirstChild();
  DCHECK(!child || !child->NextSibling());

  if (IsMarkerImage(marker)) {
    StyleImage* list_style_image = list_item->StyleRef().ListStyleImage();
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
          LayoutNGListMarkerImage::CreateAnonymous(&marker.GetDocument());
      scoped_refptr<ComputedStyle> image_style =
          ComputedStyle::CreateAnonymousStyleWithDisplay(marker.StyleRef(),
                                                         EDisplay::kInline);
      image->SetStyle(image_style);
      image->SetImageResource(
          MakeGarbageCollected<LayoutImageResourceStyleImage>(
              list_style_image));
      image->SetIsGeneratedContent();
      marker.AddChild(image);
    }
    marker_text_type_ = kNotText;
    return;
  }

  if (list_item->StyleRef().ListStyleType() == EListStyleType::kNone) {
    marker_text_type_ = kNotText;
    return;
  }

  // Create a LayoutText in it.
  LayoutText* text = nullptr;
  // |text_style| should be as same as style propagated in
  // |LayoutObject::PropagateStyleToAnonymousChildren()| to avoid unexpected
  // full layout due by style difference. See http://crbug.com/980399
  scoped_refptr<ComputedStyle> text_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(
          marker.StyleRef(), marker.StyleRef().Display());
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
    text = LayoutText::CreateEmptyAnonymous(marker.GetDocument(), text_style,
                                            LegacyLayout::kAuto);
    marker.AddChild(text);
    marker_text_type_ = kUnresolved;
  }
}

LayoutObject* ListMarker::SymbolMarkerLayoutText(
    const LayoutObject& marker) const {
  if (marker_text_type_ != kSymbolValue)
    return nullptr;
  return marker.SlowFirstChild();
}

}  // namespace blink
