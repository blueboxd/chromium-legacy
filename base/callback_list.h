// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CALLBACK_LIST_H_
#define BASE_CALLBACK_LIST_H_

#include <algorithm>
#include <list>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"

// OVERVIEW:
//
// A container for a list of callbacks. Provides callers the ability to manually
// or automatically unregister callbacks at any time, including during callback
// notification.
//
// TYPICAL USAGE:
//
// class MyWidget {
//  public:
//   using CallbackList = base::RepeatingCallbackList<void(const Foo&)>;
//
//   // Registers |cb| to be called whenever NotifyFoo() is executed.
//   std::unique_ptr<CallbackList::Subscription>
//   RegisterCallback(CallbackList::CallbackType cb) {
//     return callback_list_.Add(std::move(cb));
//   }
//
//  private:
//   // Calls all registered callbacks, with |foo| as the supplied arg.
//   void NotifyFoo(const Foo& foo) {
//     callback_list_.Notify(foo);
//   }
//
//   CallbackList callback_list_;
// };
//
//
// class MyWidgetListener {
//  private:
//   void OnFoo(const Foo& foo) {
//     // Called whenever MyWidget::NotifyFoo() is executed, unless
//     // |foo_subscription_| has been reset().
//   }
//
//   // Automatically deregisters the callback when deleted (e.g. in
//   // ~MyWidgetListener()).
//   std::unique_ptr<MyWidget::CallbackList::Subscription> foo_subscription_ =
//       MyWidget::Get()->RegisterCallback(
//           base::BindRepeating(&MyWidgetListener::OnFoo, this));
// };
//
// UNSUPPORTED:
//
// * Calling Notify() reentrantly during callback notification.
// * Destroying the CallbackList during callback notification.
//
// Both of these are possible to support, but not currently necessary.

namespace base {

namespace internal {

template <typename T>
class CallbackListBase {
 public:
  using CallbackType = T;
  static_assert(IsBaseCallback<CallbackType>::value, "");

  // A cancellation handle for callers who register callbacks. Subscription
  // destruction cancels the associated callback and is legal any time,
  // including after the destruction of the CallbackList that vends it.
  class Subscription {
   public:
    explicit Subscription(base::OnceClosure destruction_closure)
        : destruction_closure_(std::move(destruction_closure)) {}

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    ~Subscription() { std::move(destruction_closure_).Run(); }

   private:
    // Run when |this| is destroyed to notify the CallbackList the associated
    // callback should be canceled. Since this is bound using a WeakPtr to the
    // CallbackList, it will automatically no-op if the CallbackList no longer
    // exists.
    base::OnceClosure destruction_closure_;
  };

  CallbackListBase() = default;
  CallbackListBase(const CallbackListBase&) = delete;
  CallbackListBase& operator=(const CallbackListBase&) = delete;

  ~CallbackListBase() {
    // Destroying the list during iteration is unsupported and will cause a UAF.
    CHECK(!iterating_);
  }

  // Registers |cb| for future notifications. Returns a Subscription that can be
  // used to cancel |cb|.
  std::unique_ptr<Subscription> Add(CallbackType cb) WARN_UNUSED_RESULT {
    DCHECK(!cb.is_null());
    return std::make_unique<Subscription>(base::BindOnce(
        &CallbackListBase::CancelCallback, weak_ptr_factory_.GetWeakPtr(),
        callbacks_.insert(callbacks_.end(), std::move(cb))));
  }

  // Registers |removal_callback| to be run after elements are removed from the
  // list of registered callbacks.
  void set_removal_callback(const RepeatingClosure& removal_callback) {
    removal_callback_ = removal_callback;
  }

  // Returns whether the list of registered callbacks is empty. This may not be
  // called while Notify() is traversing the list (since the results could be
  // inaccurate).
  bool empty() const {
    DCHECK(!iterating_);
    return callbacks_.empty();
  }

  // Calls all registered callbacks that are not canceled beforehand. If any
  // callbacks are unregistered, notifies any registered removal callback at the
  // end.
  template <typename... RunArgs>
  void Notify(RunArgs&&... args) {
    // Calling Notify() reentrantly is currently unsupported.
    DCHECK(!iterating_);

    if (empty())
      return;  // Nothing to do.

    // Canceled callbacks should be removed from the list whenever notification
    // isn't in progress, so right now all callbacks should be valid.
    const auto callback_valid = [](const auto& cb) { return !cb.is_null(); };
    DCHECK(std::all_of(callbacks_.cbegin(), callbacks_.cend(), callback_valid));

    {
      AutoReset<bool> iterating(&iterating_, true);
      // Skip any callbacks that are canceled during iteration.
      for (auto it = callbacks_.begin(); it != callbacks_.end();
           it = std::find_if(it, callbacks_.end(), callback_valid)) {
        const auto current = it++;  // Must increment before splice() below.

        // OnceCallbacks still have Subscriptions with outstanding iterators;
        // splice() removes them from |callbacks_| without invalidating those.
        if (IsOnceCallback<CallbackType>::value)
          null_callbacks_.splice(null_callbacks_.end(), callbacks_, current);

        // Run the current callback, which may cancel it or any other callbacks.
        MoveIfOnce(*current).Run(args...);
      }
    }

    // Any null callbacks remaining in the list were canceled due to
    // Subscription destruction during iteration, and can safely be erased now.
    const size_t erased_callbacks =
        EraseIf(callbacks_, [](const auto& cb) { return cb.is_null(); });

    // Run |removal_callback_| if any callbacks were canceled. Note that we
    // cannot simply compare list sizes before and after iterating, since
    // notification may result in Add()ing new callbacks as well as canceling
    // them. Also note that if this is a OnceCallbackList, the OnceCallbacks
    // that were executed above have all been removed regardless of whether
    // they're counted in |erased_callbacks_|.
    if (removal_callback_ &&
        (erased_callbacks || IsOnceCallback<CallbackType>::value))
      removal_callback_.Run();  // May delete |this|!
  }

 private:
  using Callbacks = std::list<CallbackType>;

  // Cancels the callback pointed to by |it|, which is guaranteed to be valid.
  void CancelCallback(const typename Callbacks::iterator& it) {
    if (it->is_null()) {
      // Because at most one Subscription can point to a given callback, and
      // RepeatingCallbacks are only reset by this function, *it must be a
      // OnceCallback.  Therefore, it must have already been splice()d to
      // |null_callbacks_|, and it is safe to erase any time.
      null_callbacks_.erase(it);
    } else if (iterating_) {
      // Calling erase() here is unsafe, since the loop in Notify() may be
      // referencing this same iterator, e.g. if adjacent callbacks'
      // Subscriptions are both destroyed when the first one is Run().  Just
      // reset the callback and let Notify() clean it up at the end.
      it->Reset();
    } else {
      callbacks_.erase(it);
      if (removal_callback_)
        removal_callback_.Run();  // May delete |this|!
    }
  }

  // Holds non-null callbacks, which will be called during Notify().
  Callbacks callbacks_;

  // Holds null callbacks whose Subscriptions are still alive, so the
  // Subscriptions will still contain valid iterators.  Only needed for
  // OnceCallbacks, since RepeatingCallbacks are not canceled except by
  // Subscription destruction.
  Callbacks null_callbacks_;

  // Set while Notify() is traversing |callbacks_|.  Used primarily to avoid
  // invalidating iterators that may be in use.
  bool iterating_ = false;

  // Called after elements are removed from |callbacks_|.
  RepeatingClosure removal_callback_;

  WeakPtrFactory<CallbackListBase> weak_ptr_factory_{this};
};

}  // namespace internal

template <typename Signature>
using OnceCallbackList = internal::CallbackListBase<OnceCallback<Signature>>;
template <typename Signature>
using RepeatingCallbackList =
    internal::CallbackListBase<RepeatingCallback<Signature>>;
template <typename Signature>
using CallbackList = RepeatingCallbackList<Signature>;

// Syntactic sugar to parallel that used for Callbacks.
using OnceClosureList = OnceCallbackList<void()>;
using RepeatingClosureList = RepeatingCallbackList<void()>;
using ClosureList = CallbackList<void()>;

}  // namespace base

#endif  // BASE_CALLBACK_LIST_H_
