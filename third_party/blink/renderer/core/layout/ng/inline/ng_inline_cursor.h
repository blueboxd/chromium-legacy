// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;
class LayoutBlockFlow;
class NGFragmentItem;
class NGFragmentItems;
class NGPaintFragment;
class NGPhysicalBoxFragment;
struct PhysicalOffset;

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
  NGInlineCursor(const LayoutBlockFlow& block_flow);
  NGInlineCursor(const NGFragmentItems& items);
  NGInlineCursor(const NGPaintFragment& root_paint_fragment);

  //
  // Functions to query the current position.
  //

  bool IsAtEnd() const { return !current_item_ && !current_paint_fragment_; }
  explicit operator bool() const { return !IsAtEnd(); }

  // True if the current position is a line box.
  bool IsLineBox() const;

  // |Current*| functions return an object for the current position.
  const NGFragmentItem* CurrentItem() const { return current_item_; }
  const NGPaintFragment* CurrentPaintFragment() const {
    return current_paint_fragment_;
  }
  const NGPhysicalBoxFragment* CurrentBoxFragment() const;
  const LayoutObject* CurrentLayoutObject() const;
  const NGFragmentItems& Items() const {
    DCHECK(fragment_items_);
    return *fragment_items_;
  }

  // The offset relative to the root of the inline formatting context.
  const PhysicalOffset CurrentOffset() const;

  //
  // Functions to move the current position.
  //

  // Move the current position to the next fragment in pre-order DFS. Returns
  // |true| if the move was successful.
  void MoveToNext();

  // Same as |MoveToNext| except that this skips children even if they exist.
  void MoveToNextSkippingChildren();

  // TODO(kojii): Add more variations as needed, NextSibling,
  // NextSkippingChildren, Previous, etc.

 private:
  using ItemsSpan = base::span<const std::unique_ptr<NGFragmentItem>>;

  void SetRoot(const NGFragmentItems& items);
  void SetRoot(ItemsSpan items);
  void SetRoot(const NGPaintFragment& root_paint_fragment);

  void MoveToItem(const ItemsSpan::iterator& iter);
  void MoveToNextItem();
  void MoveToNextItemSkippingChildren();

  void MoveToParentPaintFragment();
  void MoveToNextPaintFragment();
  void MoveToNextSibilingPaintFragment();
  void MoveToNextPaintFragmentSkippingChildren();

  ItemsSpan items_;
  ItemsSpan::iterator item_iter_;
  const NGFragmentItem* current_item_ = nullptr;
  const NGFragmentItems* fragment_items_ = nullptr;

  const NGPaintFragment* root_paint_fragment_ = nullptr;
  const NGPaintFragment* current_paint_fragment_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_CURSOR_H_
