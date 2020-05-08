// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_CHECKED_PTR_H_
#define BASE_MEMORY_CHECKED_PTR_H_

#include <cstddef>  // for std::nullptr_t
#include <cstdint>  // for uintptr_t
#include <utility>  // for std::swap

namespace base {

namespace internal {
// These classes/structures are part of the CheckedPtr implementation.
// DO NOT USE THESE CLASSES DIRECTLY YOURSELF.

struct CheckedPtrNoOpImpl {
  // Wraps a pointer, and returns its uintptr_t representation.
  static inline uintptr_t WrapRawPtr(const void* const_ptr) {
    return reinterpret_cast<uintptr_t>(const_ptr);
  }

  // Returns equivalent of |WrapRawPtr(nullptr)|. Separated out to make it a
  // constexpr.
  static constexpr uintptr_t GetWrappedNullPtr() {
    // This relies on nullptr and 0 being equal in the eyes of reinterpret_cast,
    // which apparently isn't true in all environments.
    return 0;
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function is allowed to crash on nullptr.
  static inline void* SafelyUnwrapPtr(uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function must handle nullptr gracefully.
  static inline void* SafelyUnwrapPtrMayBeNull(uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Advance the wrapped pointer by |delta| bytes.
  static uintptr_t Advance(uintptr_t wrapped_ptr, ptrdiff_t delta) {
    return wrapped_ptr + delta;
  }

  // Checks if wrapped pointers are equal. The pointers can be nullptr.
  static inline bool AreEqual(uintptr_t wrapped_ptr1, uintptr_t wrapped_ptr2) {
    return wrapped_ptr1 == wrapped_ptr2;
  }

  // Checks if a wrapped pointer is equal to a raw pointer. The pointers
  // can be nullptr.
  static inline bool AreEqual(uintptr_t wrapped_ptr, const void* ptr) {
    return reinterpret_cast<void*>(wrapped_ptr) == ptr;
  }
};

template <typename T>
struct DereferencedPointerType {
  using Type = decltype(*std::declval<T*>());
};
// This explicitly doesn't define any type aliases, since dereferencing void is
// invalid.
template <>
struct DereferencedPointerType<void> {};

}  // namespace internal

// DO NOT USE! EXPERIMENTAL ONLY! This is helpful for local testing!
//
// CheckedPtr is meant to be a pointer wrapper, that will crash on
// Use-After-Free (UaF) to prevent security issues. This is very much in the
// experimental phase. More context in:
// https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg
//
// For now, CheckedPtr is a no-op wrapper to aid local testing.
//
// Goals for this API:
// 1. Minimize amount of caller-side changes as much as physically possible.
// 2. Keep this class as small as possible, while still satisfying goal #1 (i.e.
//    we aren't striving to maximize compatibility with raw pointers, merely
//    adding support for cases encountered so far).
template <typename T>
class CheckedPtr {
  using Impl = internal::CheckedPtrNoOpImpl;

 public:
  constexpr CheckedPtr() noexcept = default;
  // Deliberately implicit, because CheckedPtr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr CheckedPtr(std::nullptr_t) noexcept
      : wrapped_ptr_(Impl::GetWrappedNullPtr()) {}
  // Deliberately implicit, because CheckedPtr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  CheckedPtr(T* p) noexcept : wrapped_ptr_(Impl::WrapRawPtr(p)) {}

  // In addition to nullptr_t ctor above, CheckedPtr needs to have these
  // as |=default| or |constexpr| to avoid hitting -Wglobal-constructors in
  // cases like this:
  //     struct SomeStruct { int int_field; CheckedPtr<int> ptr_field; };
  //     SomeStruct g_global_var = { 123, nullptr };
  CheckedPtr(const CheckedPtr&) noexcept = default;
  CheckedPtr(CheckedPtr&&) noexcept = default;
  CheckedPtr& operator=(const CheckedPtr&) noexcept = default;
  CheckedPtr& operator=(CheckedPtr&&) noexcept = default;

  CheckedPtr& operator=(T* p) noexcept {
    wrapped_ptr_ = Impl::WrapRawPtr(p);
    return *this;
  }

  ~CheckedPtr() = default;

  // Avoid using. The goal of CheckedPtr is to be as close to raw pointer as
  // possible, so use it only if absolutely necessary (e.g. for const_cast).
  T* get() const { return GetMayBeNull(); }

  explicit operator bool() const {
    return wrapped_ptr_ != Impl::GetWrappedNullPtr();
  }

  // Use SFINAE to avoid defining |operator*| for T=void, which wouldn't compile
  // due to |void&|.
  template <typename U = T,
            typename V = typename internal::DereferencedPointerType<U>::Type>
  V& operator*() const {
    return *GetOkToCrashOnNull();
  }
  T* operator->() const { return GetOkToCrashOnNull(); }
  // Deliberately implicit, because CheckedPtr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  operator T*() const { return GetMayBeNull(); }
  template <typename U>
  explicit operator U*() const {
    return static_cast<U*>(GetMayBeNull());
  }

  CheckedPtr& operator++() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, sizeof(T));
    return *this;
  }

  CheckedPtr& operator--() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, -sizeof(T));
    return *this;
  }

  CheckedPtr& operator+=(ptrdiff_t delta_elems) {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, delta_elems * sizeof(T));
    return *this;
  }

  CheckedPtr& operator-=(ptrdiff_t delta_elems) {
    return *this += -delta_elems;
  }

  bool operator==(T* p) const { return Impl::AreEqual(wrapped_ptr_, p); }
  bool operator!=(T* p) const { return !operator==(p); }
  bool operator==(const CheckedPtr& other) const {
    return Impl::AreEqual(wrapped_ptr_, other.wrapped_ptr_);
  }
  bool operator!=(const CheckedPtr& other) const { return !operator==(other); }
  template <typename U>
  bool operator==(const CheckedPtr<U>& other) const {
    // TODO(bartekn): Eliminate unwrapping |other| (which cast does), because it
    // may check if the pointer was freed.
    return Impl::AreEqual(wrapped_ptr_, static_cast<T*>(other));
  }
  template <typename U>
  bool operator!=(const CheckedPtr<U>& other) const {
    return !operator==(other);
  }

  void swap(CheckedPtr& other) noexcept {
    std::swap(wrapped_ptr_, other.wrapped_ptr_);
  }

 private:
  // This getter is meant for situations where the pointers is merely read, not
  // necessarily with intention to dereference. It doesn't crash on nullptr.
  T* GetMayBeNull() const {
    return static_cast<T*>(Impl::SafelyUnwrapPtrMayBeNull(wrapped_ptr_));
  }
  // This getter is meant for situations where the pointers is meant to be
  // dereferenced. It is allowed to crash on nullptr (it may or may not),
  // because it knows that the caller will crash on nullptr.
  T* GetOkToCrashOnNull() const {
    return static_cast<T*>(Impl::SafelyUnwrapPtr(wrapped_ptr_));
  }

  // Store the pointer as |uintptr_t|, because depending on implementation, its
  // unused bits may be re-purposed to store extra information.
  uintptr_t wrapped_ptr_ = Impl::GetWrappedNullPtr();
};

template <typename T>
void swap(CheckedPtr<T>& lhs, CheckedPtr<T>& rhs) noexcept {
  lhs.swap(rhs);
}

}  // namespace base

using base::CheckedPtr;

#endif  // BASE_MEMORY_CHECKED_PTR_H_
