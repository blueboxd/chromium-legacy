// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

namespace blink {

class NGFragmentItemsBuilder;

// Represents the inside of an inline formatting context.
//
// During the layout phase, descendants of the inline formatting context is
// transformed to a flat list of |NGFragmentItem| and stored in this class.
class CORE_EXPORT NGFragmentItems {
 public:
  explicit NGFragmentItems(NGFragmentItemsBuilder* builder);
  ~NGFragmentItems();

  wtf_size_t Size() const { return size_; }

  using Span = base::span<const scoped_refptr<const NGFragmentItem>>;
  Span Items() const { return base::make_span(ItemsData(), size_); }
  bool IsSubSpan(const Span& span) const;

  const String& Text(bool first_line) const {
    return UNLIKELY(first_line) ? first_line_text_content_ : text_content_;
  }

  // Associate |NGFragmentItem|s with |LayoutObject|s and finalize the items
  // (set which ones are the first / last for the LayoutObject).
  static void FinalizeAfterLayout(
      const Vector<scoped_refptr<const NGLayoutResult>, 1>&);

  // Disassociate |NGFragmentItem|s with |LayoutObject|s. And more.
  static void ClearAssociatedFragments(LayoutObject* container);

  // Notify when |LayoutObject| will be destroyed/moved.
  static void LayoutObjectWillBeDestroyed(const LayoutObject& layout_object);
  static void LayoutObjectWillBeMoved(const LayoutObject& layout_object);

  // The byte size of this instance.
  constexpr static wtf_size_t ByteSizeFor(wtf_size_t count) {
    return sizeof(NGFragmentItems) + count * sizeof(items_[0]);
  }
  wtf_size_t ByteSize() const { return ByteSizeFor(Size()); }

 private:
  const scoped_refptr<const NGFragmentItem>* ItemsData() const {
    return reinterpret_cast<const scoped_refptr<const NGFragmentItem>*>(items_);
  }

  String text_content_;
  String first_line_text_content_;

  wtf_size_t size_;

  // Semantically, |items_| is a flexible array of |scoped_refptr<const
  // NGFragmentItem>|, but |scoped_refptr| has non-trivial destruction which
  // causes an error in clang. Declare as a flexible array of |NGFragmentItem*|
  // instead. Please see |ItemsData()|.
  static_assert(
      sizeof(NGFragmentItem*) == sizeof(scoped_refptr<const NGFragmentItem>),
      "scoped_refptr must be the size of a pointer for |ItemsData()| to work");
  NGFragmentItem* items_[];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_H_
