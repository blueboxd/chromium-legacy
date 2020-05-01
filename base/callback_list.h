// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CALLBACK_LIST_H_
#define BASE_CALLBACK_LIST_H_

#include <list>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

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
//   using CallbackList = base::CallbackList<void(const Foo&)>;
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

namespace base {

namespace internal {

template <typename CallbackType>
class CallbackListBase {
 public:
  // A cancellation handle for callers who register callbacks. Subscription
  // destruction cancels the associated callback and is legal any time,
  // including after the destruction of the CallbackList that vends it.
  class Subscription {
   public:
    explicit Subscription(base::OnceClosure subscription_destroyed)
        : subscription_destroyed_(std::move(subscription_destroyed)) {}

    ~Subscription() { std::move(subscription_destroyed_).Run(); }

    // Returns true if the CallbackList associated with this subscription has
    // been deleted, which means that the associated callback will no longer be
    // invoked.
    bool IsCancelled() const { return subscription_destroyed_.IsCancelled(); }

   private:
    // Run when |this| is destroyed to notify the CallbackList the associated
    // callback should be canceled. Since this is bound using a WeakPtr to the
    // CallbackList, it will automatically no-op if the CallbackList no longer
    // exists.
    base::OnceClosure subscription_destroyed_;

    DISALLOW_COPY_AND_ASSIGN(Subscription);
  };

  // Registers |cb| for future notifications. Returns a Subscription that can be
  // used to cancel |cb|.
  std::unique_ptr<Subscription> Add(const CallbackType& cb) WARN_UNUSED_RESULT {
    DCHECK(!cb.is_null());
    return std::make_unique<Subscription>(
        base::BindOnce(&CallbackListBase::OnSubscriptionDestroyed,
                       weak_ptr_factory_.GetWeakPtr(),
                       callbacks_.insert(callbacks_.end(), cb)));
  }

  // Registers |removal_callback| to be run after elements are removed from the
  // list of registered callbacks.
  void set_removal_callback(const RepeatingClosure& callback) {
    removal_callback_ = callback;
  }

  // Returns whether the list of registered callbacks is empty. This may not be
  // called while Notify() is traversing the list (since the results could be
  // inaccurate).
  bool empty() {
    DCHECK_EQ(0u, active_iterator_count_);
    return callbacks_.empty();
  }

 protected:
  // An iterator class that can be used to access the list of callbacks.
  class Iterator {
   public:
    explicit Iterator(CallbackListBase<CallbackType>* list)
        : list_(list),
          list_iter_(list_->callbacks_.begin()) {
      ++list_->active_iterator_count_;
    }

    Iterator(const Iterator& iter)
        : list_(iter.list_),
          list_iter_(iter.list_iter_) {
      ++list_->active_iterator_count_;
    }

    ~Iterator() {
      if (list_ && --list_->active_iterator_count_ == 0) {
        // Any null callbacks remaining in the list were canceled due to
        // Subscription destruction during iteration, and can safely be erased
        // now.
        list_->Compact();
      }
    }

    CallbackType* GetNext() {
      // Skip any callbacks that are canceled during iteration.
      while ((list_iter_ != list_->callbacks_.end()) && list_iter_->is_null())
        ++list_iter_;

      CallbackType* cb = nullptr;
      if (list_iter_ != list_->callbacks_.end()) {
        cb = &(*list_iter_);
        ++list_iter_;
      }
      return cb;
    }

   private:
    CallbackListBase<CallbackType>* list_;
    typename std::list<CallbackType>::iterator list_iter_;
  };

  CallbackListBase() = default;

  ~CallbackListBase() { DCHECK_EQ(0u, active_iterator_count_); }

  // Returns an instance of a CallbackListBase::Iterator which can be used
  // to run callbacks.
  Iterator GetIterator() {
    return Iterator(this);
  }

  // Compact the list: remove any entries which were nulled out during
  // iteration.
  void Compact() {
    auto it = callbacks_.begin();
    bool updated = false;
    while (it != callbacks_.end()) {
      if ((*it).is_null()) {
        updated = true;
        it = callbacks_.erase(it);
      } else {
        ++it;
      }
    }

    // Run |removal_callback_| if any callbacks were canceled. Note that we
    // cannot simply compare list sizes before and after iterating, since
    // notification may result in Add()ing new callbacks as well as canceling
    // them.
    if (updated && !removal_callback_.is_null())
      removal_callback_.Run();  // May delete |this|!
  }

 private:
  // Cancels the callback pointed to by |iter|, which is guaranteed to be valid.
  void OnSubscriptionDestroyed(
      const typename std::list<CallbackType>::iterator& iter) {
    if (active_iterator_count_) {
      // Calling erase() here is unsafe, since the loop in Notify() may be
      // referencing this same iterator, e.g. if adjacent callbacks'
      // Subscriptions are both destroyed when the first one is Run().  Just
      // reset the callback and let Notify() clean it up at the end.
      iter->Reset();
    } else {
      callbacks_.erase(iter);
      if (removal_callback_)
        removal_callback_.Run();  // May delete |this|!
    }
  }

  // Holds non-null callbacks, which will be called during Notify().
  std::list<CallbackType> callbacks_;

  // Incremented while Notify() is traversing |callbacks_|.  Used primarily to
  // avoid invalidating iterators that may be in use.
  size_t active_iterator_count_ = 0;

  // Called after elements are removed from |callbacks_|.
  RepeatingClosure removal_callback_;

  WeakPtrFactory<CallbackListBase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CallbackListBase);
};

}  // namespace internal

template <typename Sig> class CallbackList;

template <typename... Args>
class CallbackList<void(Args...)>
    : public internal::CallbackListBase<RepeatingCallback<void(Args...)>> {
 public:
  using CallbackType = RepeatingCallback<void(Args...)>;

  CallbackList() = default;

  // Calls all registered callbacks that are not canceled beforehand. If any
  // callbacks are unregistered, notifies any registered removal callback at the
  // end.
  template <typename... RunArgs>
  void Notify(RunArgs&&... args) {
    auto it = this->GetIterator();
    CallbackType* cb;
    while ((cb = it.GetNext()) != nullptr) {
      // Run the current callback, which may cancel it or any other callbacks.
      cb->Run(args...);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackList);
};

}  // namespace base

#endif  // BASE_CALLBACK_LIST_H_
