// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/abstract_promise.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/sequenced_task_runner.h"
#include "base/task/promise/dependent_list.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace base {
namespace internal {

AbstractPromise::~AbstractPromise() {
#if DCHECK_IS_ON()
  CheckedAutoLock lock(GetCheckedLock());

  DCHECK(!must_catch_ancestor_that_could_reject_ ||
         passed_catch_responsibility_)
      << "Promise chain ending at " << from_here_.ToString()
      << " didn't have a catch for potentially rejecting promise here "
      << must_catch_ancestor_that_could_reject_->from_here().ToString();

  DCHECK(!this_must_catch_ || passed_catch_responsibility_)
      << "Potentially rejecting promise at " << from_here_.ToString()
      << " doesn't have a catch .";
#endif
}

bool AbstractPromise::IsCanceled() const {
  if (dependents_.IsCanceled())
    return true;

  const Executor* executor = GetExecutor();
  return executor && executor->IsCancelled();
}

const AbstractPromise* AbstractPromise::FindNonCurriedAncestor() const {
  const AbstractPromise* promise = this;
  while (promise->IsResolvedWithPromise()) {
    promise =
        unique_any_cast<scoped_refptr<AbstractPromise>>(promise->value_).get();
  }
  return promise;
}

void AbstractPromise::AddAsDependentForAllPrerequisites() {
  if (!prerequisites_)
    return;

  // Note a curried promise will eventually get to all its children and pass
  // them catch responsibility through AddAsDependentForAllPrerequisites,
  // although that'll be done lazily (only once they resolve/reject, so there
  // is a possibility the DCHECKs might be racy.

  for (AdjacencyListNode& node : prerequisites_->prerequisite_list) {
    node.dependent_node.dependent = this;

    // If |node.prerequisite| was canceled then early out because
    // |prerequisites_->prerequisite_list| will have been cleared.
    if (!node.prerequisite->InsertDependentOnAnyThread(&node.dependent_node))
      break;
  }
}

bool AbstractPromise::InsertDependentOnAnyThread(DependentList::Node* node) {
  scoped_refptr<AbstractPromise>& dependent = node->dependent;

  // Used to ensure no reference to the dependent is kept in case the Promise is
  // already settled.
  scoped_refptr<AbstractPromise> dependent_to_release;

#if DCHECK_IS_ON()
  {
    CheckedAutoLock lock(GetCheckedLock());
    node->dependent->MaybeInheritChecks(this);
  }
#endif

  // If |dependents_| has been consumed (i.e. this promise has been resolved
  // or rejected) then |node| may be ready to run now.
  switch (dependents_.Insert(node)) {
    case DependentList::InsertResult::SUCCESS:
      break;

    case DependentList::InsertResult::FAIL_PROMISE_RESOLVED:
      dependent_to_release = std::move(dependent);
      dependent_to_release->OnPrerequisiteResolved(this);
      break;

    case DependentList::InsertResult::FAIL_PROMISE_REJECTED:
      dependent_to_release = std::move(dependent);
      dependent_to_release->OnPrerequisiteRejected(this);
      break;

    case DependentList::InsertResult::FAIL_PROMISE_CANCELED:
      dependent_to_release = std::move(dependent);
      return dependent_to_release->OnPrerequisiteCancelled();
  }

  return true;
}

void AbstractPromise::IgnoreUncaughtCatchForTesting() {
#if DCHECK_IS_ON()
  CheckedAutoLock lock(GetCheckedLock());
  passed_catch_responsibility_ = true;
#endif
}

#if DCHECK_IS_ON()

// static
CheckedLock& AbstractPromise::GetCheckedLock() {
  static base::NoDestructor<CheckedLock> instance;
  return *instance;
}

void AbstractPromise::DoubleMoveDetector::CheckForDoubleMoveErrors(
    const base::Location& new_dependent_location,
    Executor::ArgumentPassingType new_dependent_executor_type) {
  switch (new_dependent_executor_type) {
    case Executor::ArgumentPassingType::kNoCallback:
      return;

    case Executor::ArgumentPassingType::kNormal:
      DCHECK(!dependent_move_only_promise_)
          << "Can't mix move only and non-move only " << callback_type_
          << "callback arguments for the same " << callback_type_
          << " prerequisite. See " << new_dependent_location.ToString()
          << " and " << dependent_move_only_promise_->ToString()
          << " with common ancestor " << from_here_.ToString();
      dependent_normal_promise_ =
          std::make_unique<Location>(new_dependent_location);
      return;

    case Executor::ArgumentPassingType::kMove:
      DCHECK(!dependent_move_only_promise_ ||
             *dependent_move_only_promise_ == new_dependent_location)
          << "Can't have multiple move only " << callback_type_
          << " callbacks for same " << callback_type_ << " prerequisite. See "
          << new_dependent_location.ToString() << " and "
          << dependent_move_only_promise_->ToString() << " with common "
          << callback_type_ << " prerequisite " << from_here_.ToString();
      DCHECK(!dependent_normal_promise_)
          << "Can't mix move only and non-move only " << callback_type_
          << " callback arguments for the same " << callback_type_
          << " prerequisite. See " << new_dependent_location.ToString()
          << " and " << dependent_normal_promise_->ToString() << " with common "
          << callback_type_ << " prerequisite " << from_here_.ToString();
      dependent_move_only_promise_ =
          std::make_unique<Location>(new_dependent_location);
      return;
  }
}

void AbstractPromise::MaybeInheritChecks(AbstractPromise* prerequisite) {
  if (!ancestor_that_could_resolve_) {
    // Inherit |prerequisite|'s resolve ancestor if it doesn't have a resolve
    // callback.
    if (prerequisite->resolve_argument_passing_type_ ==
        Executor::ArgumentPassingType::kNoCallback) {
      ancestor_that_could_resolve_ = prerequisite->ancestor_that_could_resolve_;
    }

    // If |prerequisite| didn't have a resolve callback (but it's reject
    // callback could resolve) or if
    // |prerequisite->ancestor_that_could_resolve_| is null then assign
    // |prerequisite->this_resolve_|.
    if (!ancestor_that_could_resolve_ && prerequisite->executor_can_resolve_)
      ancestor_that_could_resolve_ = prerequisite->this_resolve_;
  }

  if (!ancestor_that_could_reject_) {
    // Inherit |prerequisite|'s reject ancestor if it doesn't have a Catch.
    if (prerequisite->reject_argument_passing_type_ ==
        Executor::ArgumentPassingType::kNoCallback) {
      ancestor_that_could_reject_ = prerequisite->ancestor_that_could_reject_;
    }

    // If |prerequisite| didn't have a reject callback (but it's resolve
    // callback could reject) or if
    // |prerequisite->ancestor_that_could_resolve_| is null then assign
    // |prerequisite->this_reject_|.
    if (!ancestor_that_could_reject_ && prerequisite->executor_can_reject_)
      ancestor_that_could_reject_ = prerequisite->this_reject_;
  }

  if (!must_catch_ancestor_that_could_reject_) {
    // Inherit |prerequisite|'s must catch ancestor if it doesn't have a Catch.
    if (prerequisite->reject_argument_passing_type_ ==
        Executor::ArgumentPassingType::kNoCallback) {
      must_catch_ancestor_that_could_reject_ =
          prerequisite->must_catch_ancestor_that_could_reject_;
    }

    // If |prerequisite| didn't have a reject callback (but it's resolve
    // callback could reject) or if
    // |prerequisite->must_catch_ancestor_that_could_reject_| is null then
    // assign |prerequisite->this_must_catch_|.
    if (!must_catch_ancestor_that_could_reject_ &&
        prerequisite->executor_can_reject_) {
      must_catch_ancestor_that_could_reject_ = prerequisite->this_must_catch_;
    }
  }

  if (ancestor_that_could_resolve_) {
    ancestor_that_could_resolve_->CheckForDoubleMoveErrors(
        from_here_, resolve_argument_passing_type_);
  }

  if (ancestor_that_could_reject_) {
    ancestor_that_could_reject_->CheckForDoubleMoveErrors(
        from_here_, reject_argument_passing_type_);
  }

  prerequisite->passed_catch_responsibility_ = true;
}

AbstractPromise::LocationRef::LocationRef(const Location& from_here)
    : from_here_(from_here) {}

AbstractPromise::LocationRef::~LocationRef() = default;

AbstractPromise::DoubleMoveDetector::DoubleMoveDetector(
    const Location& from_here,
    const char* callback_type)
    : from_here_(from_here), callback_type_(callback_type) {}

AbstractPromise::DoubleMoveDetector::~DoubleMoveDetector() = default;

#endif

const AbstractPromise::Executor* AbstractPromise::GetExecutor() const {
  return base::unique_any_cast<Executor>(&value_);
}

AbstractPromise::Executor::PrerequisitePolicy
AbstractPromise::GetPrerequisitePolicy() {
  Executor* executor = GetExecutor();
  if (!executor) {
    // If there's no executor it's because the promise has already run. We
    // can't run again however. The only circumstance in which we expect
    // GetPrerequisitePolicy() to be called after execution is when it was
    // resolved with a promise.
    DCHECK(IsResolvedWithPromise());
    return Executor::PrerequisitePolicy::kNever;
  }
  return executor->GetPrerequisitePolicy();
}

AbstractPromise* AbstractPromise::GetFirstSettledPrerequisite() const {
  if (!prerequisites_)
    return nullptr;
  return reinterpret_cast<AbstractPromise*>(
      prerequisites_->first_settled_prerequisite.load(
          std::memory_order_acquire));
}

void AbstractPromise::Execute() {
  if (IsCanceled()) {
    OnCanceled();
    return;
  }

#if DCHECK_IS_ON()
  // Clear |must_catch_ancestor_that_could_reject_| if we can catch it.
  if (reject_argument_passing_type_ !=
      Executor::ArgumentPassingType::kNoCallback) {
    CheckedAutoLock lock(GetCheckedLock());
    must_catch_ancestor_that_could_reject_ = nullptr;
  }
#endif

  DCHECK(!IsResolvedWithPromise());
  DCHECK(GetExecutor()) << from_here_.ToString() << " value_ contains "
                        << value_.type();

  // This is likely to delete the executor.
  GetExecutor()->Execute(this);
}

bool AbstractPromise::DispatchIfNonCurriedRootSettled() {
  AbstractPromise* curried_root = FindNonCurriedAncestor();
  if (!curried_root->IsSettled())
    return false;

#if DCHECK_IS_ON()
  {
    CheckedAutoLock lock(GetCheckedLock());
    MaybeInheritChecks(curried_root);
  }
#endif

  if (curried_root->IsResolved()) {
    OnResolveDispatchReadyDependents();
  } else if (curried_root->IsRejected()) {
    OnRejectDispatchReadyDependents();
  } else {
    DCHECK(curried_root->IsCanceled());
    OnPrerequisiteCancelled();
  }
  return true;
}

void AbstractPromise::OnPrerequisiteResolved(
    AbstractPromise* resolved_prerequisite) {
  DCHECK(resolved_prerequisite->IsResolved());
  if (IsResolvedWithPromise()) {
    // The executor has already run so we don't need to call
    // MarkPrerequisiteAsSettling.
    bool settled = DispatchIfNonCurriedRootSettled();
    DCHECK(settled);
    return;
  }

  switch (GetPrerequisitePolicy()) {
    case Executor::PrerequisitePolicy::kAll:
      if (prerequisites_->DecrementPrerequisiteCountAndCheckIfZero())
        DispatchPromise();
      break;

    case Executor::PrerequisitePolicy::kAny:
      // PrerequisitePolicy::kAny should resolve immediately.
      if (prerequisites_->MarkPrerequisiteAsSettling(resolved_prerequisite))
        DispatchPromise();
      break;

    case Executor::PrerequisitePolicy::kNever:
      break;
  }
}

void AbstractPromise::OnPrerequisiteRejected(
    AbstractPromise* rejected_prerequisite) {
  DCHECK(rejected_prerequisite->IsRejected());

  // Promises::All (or Race if we add that) can have multiple prerequisites and
  // it will reject as soon as any prerequisite rejects. Multiple prerequisites
  // can reject, but we wish to record only the first one. Also we can only
  // invoke executors once.
  if (prerequisites_->MarkPrerequisiteAsSettling(rejected_prerequisite) &&
      !DispatchIfNonCurriedRootSettled()) {
    DispatchPromise();
  }
}

bool AbstractPromise::OnPrerequisiteCancelled() {
  switch (GetPrerequisitePolicy()) {
    case Executor::PrerequisitePolicy::kAll:
      // PrerequisitePolicy::kAll should cancel immediately.
      OnCanceled();
      return false;

    case Executor::PrerequisitePolicy::kAny:
      // PrerequisitePolicy::kAny should only cancel if all if it's
      // pre-requisites have been canceled.
      if (prerequisites_->DecrementPrerequisiteCountAndCheckIfZero()) {
        OnCanceled();
        return false;
      }
      return true;

    case Executor::PrerequisitePolicy::kNever:
      // If we we where resolved with a promise then we can't have had
      // PrerequisitePolicy::kAny or PrerequisitePolicy::kNever before the
      // executor was replaced with the curried promise, so pass on
      // cancellation.
      if (IsResolvedWithPromise())
        OnCanceled();
      return false;
  }
}

void AbstractPromise::OnResolveDispatchReadyDependents() {
  class Visitor : public DependentList::Visitor {
   public:
    explicit Visitor(AbstractPromise* resolved_prerequisite)
        : resolved_prerequisite_(resolved_prerequisite) {}

   private:
    void Visit(scoped_refptr<AbstractPromise> dependent) override {
      dependent->OnPrerequisiteResolved(resolved_prerequisite_);
    }
    AbstractPromise* resolved_prerequisite_;
  };

  Visitor visitor(this);
  dependents_.ResolveAndConsumeAllDependents(&visitor);
}

void AbstractPromise::OnRejectDispatchReadyDependents() {
  class Visitor : public DependentList::Visitor {
   public:
    explicit Visitor(AbstractPromise* rejected_prerequisite)
        : rejected_prerequisite_(rejected_prerequisite) {}

   private:
    void Visit(scoped_refptr<AbstractPromise> dependent) override {
      dependent->OnPrerequisiteRejected(rejected_prerequisite_);
    }
    AbstractPromise* rejected_prerequisite_;
  };

  Visitor visitor(this);
  dependents_.RejectAndConsumeAllDependents(&visitor);
}

void AbstractPromise::DispatchPromise() {
  if (task_runner_) {
    task_runner_->PostPromiseInternal(this, TimeDelta());
  } else {
    Execute();
  }
}

void AbstractPromise::OnCanceled() {
  class Visitor : public DependentList::Visitor {
   private:
    void Visit(scoped_refptr<AbstractPromise> dependent) override {
      dependent->OnPrerequisiteCancelled();
    }
  };

  Visitor visitor;
  if (!dependents_.CancelAndConsumeAllDependents(&visitor))
    return;

  // The executor could be keeping a promise alive, but it's never going to run
  // so clear it.
  value_ = unique_any();

#if DCHECK_IS_ON()
  {
    CheckedAutoLock lock(GetCheckedLock());
    passed_catch_responsibility_ = true;
  }
#endif

  // We need to release any AdjacencyListNodes we own to prevent memory leaks
  // due to refcount cycles. We can't just clear |prerequisite_list| (which
  // contains DependentList::Node) because in the case of multiple prerequisites
  // they may not have all be settled, which means some will want to traverse
  // their |dependent_list| which includes this promise. This is a problem
  // because there isn't a conveniant way of removing ourself from their
  // |dependent_list|. It's sufficient however to simply null our references.
  if (prerequisites_) {
    for (AdjacencyListNode& node : prerequisites_->prerequisite_list) {
#if DCHECK_IS_ON()
      // A settled prerequisite should not keep a reference to this.
      if (node.prerequisite->IsSettled())
        DCHECK(!node.dependent_node.dependent);
#endif
      node.prerequisite = nullptr;
    }
  }
}

void AbstractPromise::OnResolved() {
#if DCHECK_IS_ON()
  DCHECK(executor_can_resolve_ || IsResolvedWithPromise())
      << from_here_.ToString();
#endif
  if (IsResolvedWithPromise()) {
    scoped_refptr<AbstractPromise> curried_promise =
        unique_any_cast<scoped_refptr<AbstractPromise>>(value_);

    if (DispatchIfNonCurriedRootSettled()) {
      prerequisites_->prerequisite_list.clear();
    } else {
      // The curried promise isn't already settled we need to throw away any
      // existing dependencies and make |curried_promise| the only dependency of
      // this promise.
#if DCHECK_IS_ON()
      {
        CheckedAutoLock lock(GetCheckedLock());
        ancestor_that_could_resolve_ = nullptr;
        ancestor_that_could_reject_ = nullptr;
      }
#endif
      if (prerequisites_) {
        prerequisites_->ResetWithSingleDependency(curried_promise);
      } else {
        prerequisites_ = std::make_unique<AdjacencyList>(curried_promise);
      }
      AddAsDependentForAllPrerequisites();
    }
  } else {
    OnResolveDispatchReadyDependents();

    // We need to release any AdjacencyListNodes we own to prevent memory leaks
    // due to refcount cycles.
    if (prerequisites_)
      prerequisites_->prerequisite_list.clear();
  }
}

void AbstractPromise::OnRejected() {
  // Rejection with a rejected promise doesn't need special handling.
  DCHECK(!IsResolvedWithPromise() ||
         unique_any_cast<scoped_refptr<AbstractPromise>>(value_)->IsRejected());
#if DCHECK_IS_ON()
  DCHECK(executor_can_reject_) << from_here_.ToString();
#endif
  OnRejectDispatchReadyDependents();

  // We need to release any AdjacencyListNodes we own to prevent memory leaks
  // due to refcount cycles. We can't just clear |prerequisite_list| (which
  // contains DependentList::Node) because in the case of multiple prerequisites
  // they may not have all be settled, which means some will want to traverse
  // their |dependent_list| which includes this promise. This is a problem
  // because there isn't a conveniant way of removing ourself from their
  // |dependent_list|. It's sufficient however to simply null our references.
  if (prerequisites_) {
    for (AdjacencyListNode& node : prerequisites_->prerequisite_list) {
#if DCHECK_IS_ON()
      // A settled prerequisite should not keep a reference to this.
      if (node.prerequisite->IsSettled())
        DCHECK(!node.dependent_node.dependent);
#endif
      node.prerequisite = nullptr;
    }
  }
}

AbstractPromise::AdjacencyListNode::AdjacencyListNode() = default;

AbstractPromise::AdjacencyListNode::AdjacencyListNode(
    scoped_refptr<AbstractPromise> promise)
    : prerequisite(std::move(promise)) {}

AbstractPromise::AdjacencyListNode::~AdjacencyListNode() = default;

AbstractPromise::AdjacencyListNode::AdjacencyListNode(
    AdjacencyListNode&& other) noexcept = default;

AbstractPromise::AdjacencyList::AdjacencyList() = default;

AbstractPromise::AdjacencyList::AdjacencyList(
    scoped_refptr<AbstractPromise> prerequisite)
    : prerequisite_list(1), action_prerequisite_count(1) {
  prerequisite_list[0].prerequisite = std::move(prerequisite);
}

AbstractPromise::AdjacencyList::AdjacencyList(
    std::vector<AdjacencyListNode> nodes)
    : prerequisite_list(std::move(nodes)),
      action_prerequisite_count(prerequisite_list.size()) {}

AbstractPromise::AdjacencyList::~AdjacencyList() = default;

bool AbstractPromise::AdjacencyList::
    DecrementPrerequisiteCountAndCheckIfZero() {
  return action_prerequisite_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
}

// For PrerequisitePolicy::kAll this is called for the first rejected
// prerequisite. For PrerequisitePolicy:kAny this is called for the first
// resolving or rejecting prerequisite.
bool AbstractPromise::AdjacencyList::MarkPrerequisiteAsSettling(
    AbstractPromise* settled_prerequisite) {
  DCHECK(settled_prerequisite->IsSettled());
  uintptr_t expected = 0;
  return first_settled_prerequisite.compare_exchange_strong(
      expected, reinterpret_cast<uintptr_t>(settled_prerequisite),
      std::memory_order_acq_rel);
}

void AbstractPromise::AdjacencyList::ResetWithSingleDependency(
    scoped_refptr<AbstractPromise> prerequisite) {
  prerequisite_list.clear();
  prerequisite_list.push_back(AdjacencyListNode{std::move(prerequisite)});
  action_prerequisite_count = 1;
}

AbstractPromise::Executor::~Executor() {
  vtable_->destructor(storage_);
}

AbstractPromise::Executor::PrerequisitePolicy
AbstractPromise::Executor::GetPrerequisitePolicy() const {
  return vtable_->get_prerequisite_policy(storage_);
}

bool AbstractPromise::Executor::IsCancelled() const {
  return vtable_->is_cancelled(storage_);
}

#if DCHECK_IS_ON()
AbstractPromise::Executor::ArgumentPassingType
AbstractPromise::Executor::ResolveArgumentPassingType() const {
  return vtable_->resolve_argument_passing_type(storage_);
}

AbstractPromise::Executor::ArgumentPassingType
AbstractPromise::Executor::RejectArgumentPassingType() const {
  return vtable_->reject_argument_passing_type(storage_);
}

bool AbstractPromise::Executor::CanResolve() const {
  return vtable_->can_resolve(storage_);
}

bool AbstractPromise::Executor::CanReject() const {
  return vtable_->can_reject(storage_);
}
#endif

void AbstractPromise::Executor::Execute(AbstractPromise* promise) {
  return vtable_->execute(storage_, promise);
}

}  // namespace internal
}  // namespace base
