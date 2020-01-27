// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_FLEXIBLE_ARRAY_BUFFER_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_FLEXIBLE_ARRAY_BUFFER_VIEW_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CORE_EXPORT FlexibleArrayBufferView {
  STACK_ALLOCATED();

 public:
  FlexibleArrayBufferView()
      : full_(nullptr), small_data_(nullptr), small_length_(0) {}

  void SetFull(DOMArrayBufferView* full) { full_ = full; }
  void SetSmall(void* data, size_t length) {
    small_data_ = data;
    small_length_ = length;
  }

  void Clear() {
    full_ = nullptr;
    small_data_ = nullptr;
    small_length_ = 0;
  }

  bool IsEmpty() const { return !full_ && !small_data_; }
  bool IsFull() const { return full_; }

  DOMArrayBufferView* Full() const {
    DCHECK(IsFull());
    return full_;
  }

  // WARNING: The pointer returned by baseAddressMaybeOnStack() may point to
  // temporary storage that is only valid during the life-time of the
  // FlexibleArrayBufferView object.
  void* BaseAddressMaybeOnStack() const {
    DCHECK(!IsEmpty());
    return IsFull() ? full_->BaseAddressMaybeShared() : small_data_;
  }

  size_t ByteLengthAsSizeT() const {
    DCHECK(!IsEmpty());
    return IsFull() ? full_->byteLengthAsSizeT() : small_length_;
  }

  unsigned DeprecatedByteLengthAsUnsigned() const {
    DCHECK(!IsEmpty());
    return IsFull() ? base::checked_cast<unsigned>(full_->byteLengthAsSizeT())
                    : base::checked_cast<unsigned>(small_length_);
  }

  operator bool() const { return !IsEmpty(); }

 private:
  DOMArrayBufferView* full_;

  void* small_data_;
  size_t small_length_;
  DISALLOW_COPY_AND_ASSIGN(FlexibleArrayBufferView);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_FLEXIBLE_ARRAY_BUFFER_VIEW_H_
