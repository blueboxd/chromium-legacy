// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StatusOr<T> is the union of a Status object and a T
// object. StatusOr models the concept of an object that is either a
// usable value, or an error Status explaining why such a value is
// not present. To this end, StatusOr<T> does not allow its Status
// value to be Status::StatusOK(). Further, StatusOr<T*> does not allow the
// contained pointer to be nullptr.
//
// The primary use-case for StatusOr<T> is as the return value of a
// function which may fail.
//
// Example client usage for a StatusOr<T>, where T is not a pointer:
//
//  StatusOr<float> result = DoBigCalculationThatCouldFail();
//  if (result.ok()) {
//    float answer = result.ValueOrDie();
//    printf("Big calculation yielded: %f", answer);
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<T*>:
//
//  StatusOr<Foo*> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo(result.ValueOrDie());
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<std::unique_ptr<T>>:
//
//  StatusOr<std::unique_ptr<Foo>> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo = result.ValueOrDie();
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example factory implementation returning StatusOr<T*>:
//
//  StatusOr<Foo*> FooFactory::MakeNewFoo(int arg) {
//    if (arg <= 0) {
//      return Status(error::INVALID_ARGUMENT, "Arg must be positive");
//    } else {
//      return new Foo(arg);
//    }
//  }
//

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_STATUSOR_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_STATUSOR_H_

#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"

namespace reporting {

template <typename T>
class WARN_UNUSED_RESULT StatusOr {
  template <typename U>
  friend class StatusOr;

 public:
  // Construct a new StatusOr with UNKNOWN status and no value.
  StatusOr();

  // Construct a new StatusOr with the given non-ok status. After calling
  // this constructor, calls to ValueOrDie() will CHECK-fail.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return
  // value, so it is convenient and sensible to be able to do 'return
  // Status()' when the return type is StatusOr<T>.
  //
  // REQUIRES: IsOK(status). This requirement is CHECKed.
  // In optimized builds, passing Status::StatusOK() here will have the effect
  // of passing PosixErrorSpace::EINVAL as a fallback.
  StatusOr(const Status& status);  // NOLINT

  // Construct a new StatusOr with the given value. If T is a plain pointer,
  // value must not be nullptr. After calling this constructor, calls to
  // ValueOrDie() will succeed, and calls to status() will return OK.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return type
  // so it is convenient and sensible to be able to do 'return T()'
  // when the return type is StatusOr<T>.
  //
  // REQUIRES: if T is a plain pointer, value != nullptr. This requirement is
  // CHECKed. In optimized builds, passing a null pointer here will have
  // the effect of passing PosixErrorSpace::EINVAL as a fallback.
  StatusOr(const T& value);  // NOLINT

  // Construct a new StatusOr taking ownership of given value. Value must not be
  // nullptr. After calling this constructor, calls to ValueOrDie() will
  // succeed, and calls to status() will return OK.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return type
  // so it is convenient and sensible to be able to do 'return T()'
  // when the return type is StatusOr<T>.
  StatusOr(T&& value);  // NOLINT

  // StatusOr<T> will be copy constructible/assignable if T is copy
  // constructible.
  StatusOr(const StatusOr&);
  StatusOr& operator=(const StatusOr&);

  // StatusOr<T> will be move constructible/assignable if T is move
  // constructible.
  StatusOr(StatusOr&&);
  StatusOr& operator=(StatusOr&&);

  // Conversion copy constructor, T must be copy constructible from U
  template <typename U>
  StatusOr(const StatusOr<U>& other);

  // Conversion assignment operator, T must be assignable from U
  template <typename U>
  StatusOr& operator=(const StatusOr<U>& other);

  // Returns a reference to our status. If this contains a T, then
  // returns Status::StatusOK().
  const Status& status() const;

  // Returns this->status().ok()
  bool ok() const;

  // Returns a reference to our current value, or CHECK-fails if !this->ok().
  const T& ValueOrDie() const;

  // Moves and returns our current value, or CHECK-fails if !this->ok().
  // The StatusOr object is invalidated after this call and will be updated to
  // contain error::UNKNOWN error code.
  T&& ValueOrDie();

 private:
  Status status_;
  T value_;
};

////////////////////////////////////////////////////////////////////////////////
// Implementation details for StatusOr<T>

namespace internal {

class StatusOrHelper {
 public:
  // Move type-agnostic error handling to the .cc.
  static void Crash(const Status& status);
};

class StatusResetter {
 public:
  StatusResetter(Status* status, const Status& reset_to_status)
      : status_(status), reset_to_status_(reset_to_status) {}
  StatusResetter(const StatusResetter& other) = delete;
  StatusResetter& operator=(const StatusResetter& other) = delete;
  ~StatusResetter() { *status_ = reset_to_status_; }

 private:
  Status* const status_;
  const Status reset_to_status_;
};

}  // namespace internal

template <typename T>
inline StatusOr<T>::StatusOr() : status_(error::UNKNOWN, "") {}

template <typename T>
inline StatusOr<T>::StatusOr(const Status& status) {
  if (status.ok()) {
    status_ =
        Status(error::INTERNAL, "Status::StatusOK() is not a valid argument.");
  } else {
    status_ = status;
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const T& value) {
  if (std::is_pointer<T>::value && !value) {
    status_ = Status(error::INTERNAL, "nullptr is not a vaild argument.");
  } else {
    status_ = Status::StatusOK();
    value_ = value;
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(T&& value) {
  if (std::is_pointer<T>::value && !value) {
    status_ = Status(error::INTERNAL, "nullptr is not a vaild argument.");
  } else {
    status_ = Status::StatusOK();
    value_ = std::move(value);
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const StatusOr<T>& other)
    : status_(other.status_), value_(other.value_) {}

template <typename T>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr<T>& other) {
  status_ = other.status_;
  value_ = other.value_;
  return *this;
}

template <typename T>
template <typename U>
inline StatusOr<T>::StatusOr(const StatusOr<U>& other)
    : status_(other.status_), value_(other.status_.ok() ? other.value_ : T()) {}

template <typename T>
template <typename U>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr<U>& other) {
  status_ = other.status_;
  if (status_.ok())
    value_ = other.value_;
  return *this;
}

template <typename T>
inline const Status& StatusOr<T>::status() const {
  return status_;
}

template <typename T>
inline bool StatusOr<T>::ok() const {
  return status().ok();
}

template <typename T>
inline const T& StatusOr<T>::ValueOrDie() const {
  if (!status_.ok()) {
    internal::StatusOrHelper::Crash(status_);
  }
  return value_;
}

template <typename T>
inline T&& StatusOr<T>::ValueOrDie() {
  if (!status_.ok()) {
    internal::StatusOrHelper::Crash(status_);
  }
  internal::StatusResetter resetter(&status_,
                                    Status(error::UNKNOWN, "Value moved out"));
  return std::move(value_);
}

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_STATUSOR_H_
