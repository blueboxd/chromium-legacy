// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/dependent_list.h"

#include <algorithm>
#include <atomic>
#include <cstdint>

#include "base/logging.h"
#include "base/task/promise/abstract_promise.h"

namespace base {
namespace internal {
namespace {

DependentList::Node* ReverseList(DependentList::Node* list) {
  DependentList::Node* prev = nullptr;
  while (list) {
    DependentList::Node* next = list->next;
    list->next = prev;
    prev = list;
    list = next;
  }
  return prev;
}

// Goes through the list starting at |head| consuming node->dependent and
// passing it to the provided |visitor|.
void DispatchAll(DependentList::Node* head, DependentList::Visitor* visitor) {
  head = ReverseList(head);
  DependentList::Node* next = nullptr;
  while (head) {
    next = head->next;
    // consume_fn might delete the node, so no access to node past this
    // call!
    visitor->Visit(std::move(head->dependent));
    head = next;
  }
}

}  // namespace

DependentList::Visitor::~Visitor() = default;

DependentList::Node::Node() = default;
DependentList::Node::Node(DependentList::Node&& other) = default;
DependentList::Node& DependentList::Node::operator=(
    DependentList::Node&& other) = default;
DependentList::Node::~Node() = default;

DependentList::DependentList(State initial_state)
    : data_(CreateData(nullptr,
                       initial_state,
                       initial_state == State::kUnresolved ? kAllowInserts
                                                           : kBlockInserts)) {}

DependentList::DependentList(ConstructUnresolved)
    : DependentList(State::kUnresolved) {}

DependentList::DependentList(ConstructResolved)
    : DependentList(State::kResolved) {}

DependentList::DependentList(ConstructRejected)
    : DependentList(State::kRejected) {}

DependentList::~DependentList() = default;

DependentList::InsertResult DependentList::Insert(Node* node) {
  DCHECK(!node->next);

  // std::memory_order_acquire for hapens-after relation with
  // SettleAndDispatchAllDependents completing and thus this this call returning
  // an error.
  uintptr_t prev_data = data_.load(std::memory_order_acquire);
  bool did_insert = false;
  while (IsAllowingInserts(prev_data) && !did_insert) {
    node->next = ExtractHead(prev_data);

    // On success std::memory_order_release so that all memory operations become
    // visible in SettleAndDispatchAllDependents when iterating the list.
    // On failure std::memory_order_acquire for happens-after relation with
    // SettleAndDispatchAllDependents completing and thus this this call
    // returning an error.
    //
    // Note: ABA is not an issue here as we do not care that head_ might now be
    // pointing to a different node (but with same address) we only need to
    // guarantee that node->next points to the current head (which is now the
    // new node but with the same address so node->next is still valid).
    if (data_.compare_exchange_weak(
            prev_data, CreateData(node, ExtractState(prev_data), kAllowInserts),
            std::memory_order_release, std::memory_order_acquire)) {
      did_insert = true;
    } else {
      // Cleanup in case the loop terminates
      node->next = nullptr;
    }
  }

  if (did_insert) {
    return InsertResult::SUCCESS;
  }

  switch (ExtractState(prev_data)) {
    case State::kResolved:
      return InsertResult::FAIL_PROMISE_RESOLVED;

    case State::kRejected:
      return InsertResult::FAIL_PROMISE_REJECTED;

    case State::kCanceled:
      return InsertResult::FAIL_PROMISE_CANCELED;

    case State::kUnresolved:
      // We must have inserted, as inserts must be allowed if in state
      // kUnresolved
      NOTREACHED();
      return InsertResult::SUCCESS;
  }
}

bool DependentList::SettleAndDispatchAllDependents(const State settled_state,
                                                   Visitor* visitor) {
  DCHECK_NE(settled_state, State::kUnresolved);

  // Whether this invocation won the settlement race. If so, it will now keep
  // dispatching all nodes in as many attempts as it takes to win the race
  // against Insert()'s.
  bool did_set_state = false;

  // This load, and the ones in for compare_exchange_weak failures can be
  // std::memory_order_relaxed as we do not make any ordering guarantee when
  // this method returns false.
  uintptr_t prev_data = data_.load(std::memory_order_relaxed);
  while (true) {
    if (!did_set_state && ExtractState(prev_data) != State::kUnresolved) {
      // Somebody else set the state.
      return false;
    }

    if (!IsListEmpty(prev_data)) {
      // List is not empty and we might have set the state previously
      // We need to:
      //  * Settle the state (or leave as is if we already did).
      //  * allow_inserts. As we are going to dispatch the nodes we need to
      //    make sure that other threads can still insert to the list
      //    (instead of just resolving themselves) to preserve dispatch order.
      //  * Take ownership of all nodes currently in the queue, i.e. set the
      //    head to nullptr, marking it empty
      DCHECK_EQ(ExtractState(prev_data),
                did_set_state ? settled_state : State::kUnresolved);
      DCHECK(IsAllowingInserts(prev_data));
      uintptr_t new_data = CreateData(nullptr, settled_state, kAllowInserts);

      // On success std::memory_order_acquire for happens-after relation with
      // with the last successful Insert().
      if (!data_.compare_exchange_weak(prev_data, new_data,
                                       std::memory_order_acquire,
                                       std::memory_order_relaxed)) {
        continue;
      }
      did_set_state = true;
      DispatchAll(ExtractHead(prev_data), visitor);
      prev_data = new_data;
    }

    // List is empty and we might have set the state previously, so we
    // can settle the state (or leave as is if we already did) and freeze
    // the list.
    DCHECK(IsListEmpty(prev_data));
    DCHECK_EQ(ExtractState(prev_data),
              did_set_state ? settled_state : State::kUnresolved);
    // On success std::memory_order_release for happens-before relation with
    // Insert returning an error.
    if (data_.compare_exchange_weak(
            prev_data, CreateData(nullptr, settled_state, kBlockInserts),
            std::memory_order_release, std::memory_order_relaxed)) {
      // Inserts no longer allowed, state settled and list is empty. We are
      // done!
      return true;
    }
  }
}

// The following methods do not make any ordering guarantees, false return
// values are always stale. A true return value just means that
// SettleAndDispatchAllDependents was called but it gives no guarantees as
// whether any of the nodes was dispatched or the call finished. Thus
// std::memory_order_relaxed.

bool DependentList::IsSettled() const {
  return ExtractState(data_.load(std::memory_order_relaxed)) !=
         State::kUnresolved;
}

bool DependentList::IsResolved() const {
  return ExtractState(data_.load(std::memory_order_relaxed)) ==
         State::kResolved;
}

bool DependentList::IsRejected() const {
  return ExtractState(data_.load(std::memory_order_relaxed)) ==
         State::kRejected;
}

bool DependentList::IsCanceled() const {
  return ExtractState(data_.load(std::memory_order_relaxed)) ==
         State::kCanceled;
}

}  // namespace internal
}  // namespace base
