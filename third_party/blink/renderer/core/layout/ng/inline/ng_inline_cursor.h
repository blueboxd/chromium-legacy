// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;
class LayoutBlockFlow;
class NGFragmentItem;
class NGFragmentItems;
class NGPaintFragment;

// This class traverses fragments in an inline formatting context.
//
// When constructed, the initial position is empty. Call |MoveToNext()| to move
// to the first fragment.
//
// TODO(kojii): |NGPaintFragment| should be gone when |NGPaintFragment| is
// deprecated and all its uses are removed.
class CORE_EXPORT NGInlineCursor {
  STACK_ALLOCATED();

 public:
  NGInlineCursor(LayoutBlockFlow*);

  //
  // Functions to query the current position.
  //

  // True if the current position is a line box.
  bool IsLineBox() const;

  // |Current*| functions return an object for the current position.
  const NGFragmentItem* CurrentItem() const { return current_item_; }
  const NGPaintFragment* CurrentPaintFragment() const {
    return current_paint_fragment_;
  }
  const LayoutObject* CurrentLayoutObject() const;

  //
  // Functions to move the current position.
  //

  // Move the current position to the next fragment in pre-order DFS. Returns
  // |true| if the move was successful.
  bool MoveToNext();

  // Same as |MoveToNext| except that this skips children even if they exist.
  bool MoveToNextSkippingChildren();

  // TODO(kojii): Add more variations as needed, NextSibling,
  // NextSkippingChildren, Previous, etc.

 private:
  void SetRoot(const NGFragmentItems* items);
  void SetRoot(const NGPaintFragment* root_paint_fragment);

  bool MoveToItem(unsigned item_index);
  bool MoveToNextItem();
  bool MoveToNextItemSkippingChildren();

  bool MoveToParentPaintFragment();
  bool MoveToNextPaintFragment();
  bool MoveToNextSibilingPaintFragment();
  bool MoveToNextPaintFragmentSkippingChildren();

  const NGFragmentItems* items_ = nullptr;
  const NGPaintFragment* root_paint_fragment_ = nullptr;

  const NGFragmentItem* current_item_ = nullptr;
  const NGPaintFragment* current_paint_fragment_ = nullptr;

  unsigned current_item_index_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_
