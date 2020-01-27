// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LIST_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"

namespace blink {

// This class holds code shared among LayoutNG classes for list markers.
class CORE_EXPORT ListMarker {
  friend class LayoutNGListItem;

 public:
  explicit ListMarker();

  static const ListMarker* Get(const LayoutObject*);
  static ListMarker* Get(LayoutObject*);

  static LayoutNGListItem* ListItem(const LayoutObject&);

  String MarkerTextWithSuffix(const LayoutObject&) const;
  String MarkerTextWithoutSuffix(const LayoutObject&) const;

  // Marker text with suffix, e.g. "1. ", for use in accessibility.
  String TextAlternative(const LayoutObject&) const;

  static bool IsMarkerImage(const LayoutObject& marker) {
    return ListItem(marker)->StyleRef().GeneratesMarkerImage();
  }

  void UpdateMarkerTextIfNeeded(LayoutObject& marker) {
    if (!is_marker_text_updated_ && !IsMarkerImage(marker))
      UpdateMarkerText(marker);
  }
  void UpdateMarkerContentIfNeeded(LayoutObject&);

  void OrdinalValueChanged(LayoutObject&);

  LayoutObject* SymbolMarkerLayoutText(const LayoutObject&) const;

 private:
  enum MarkerTextFormat { kWithSuffix, kWithoutSuffix };
  enum MarkerType { kStatic, kOrdinalValue, kSymbolValue };
  MarkerType MarkerText(const LayoutObject&,
                        StringBuilder*,
                        MarkerTextFormat) const;
  void UpdateMarkerText(LayoutObject&);
  void UpdateMarkerText(LayoutObject&, LayoutText*);

  void ListStyleTypeChanged(LayoutObject&);

  unsigned marker_type_ : 2;  // Type
  unsigned is_marker_text_updated_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LIST_MARKER_H_
