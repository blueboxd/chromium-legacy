// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_RECEIVER_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_RECEIVER_SET_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

// HeapMojoReceiverSet is a wrapper for mojo::ReceiverSet to be owned by a
// garbage-collected object. Blink is expected to use HeapMojoReceiverSet by
// default. HeapMojoReceiverSet must be associated with context.
// HeapMojoReceiverSet's constructor takes context as a mandatory parameter.
// HeapMojoReceiverSet resets the mojo connection when 1) the owner object is
// garbage-collected or 2) the associated ExecutionContext is detached.

// TODO(crbug.com/1058076) HeapMojoWrapperMode should be removed once we ensure
// that the interface is not used after ContextDestroyed().
template <typename Interface,
          HeapMojoWrapperMode Mode = HeapMojoWrapperMode::kWithContextObserver>
class HeapMojoReceiverSet {
  DISALLOW_NEW();

 public:
  using ImplPointerType = typename mojo::Receiver<Interface>::ImplPointerType;

  explicit HeapMojoReceiverSet(ContextLifecycleNotifier* context)
      : wrapper_(MakeGarbageCollected<Wrapper>(context)) {
    DCHECK(context);
  }

  // Methods to redirect to mojo::ReceiverSet:
  mojo::ReceiverId Add(ImplPointerType impl,
                       mojo::PendingReceiver<Interface> receiver,
                       scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(task_runner);
    return wrapper_->receiver_set().Add(std::move(impl), std::move(receiver),
                                        task_runner);
  }

  bool Remove(mojo::ReceiverId id) {
    return wrapper_->receiver_set().Remove(id);
  }

  void Clear() { wrapper_->receiver_set().Clear(); }

  bool HasReceiver(mojo::ReceiverId id) {
    return wrapper_->receiver_set().HasReceiver(id);
  }

  void Trace(Visitor* visitor) { visitor->Trace(wrapper_); }

 private:
  // Garbage collected wrapper class to add a prefinalizer.
  class Wrapper final : public GarbageCollected<Wrapper>,
                        public ContextLifecycleObserver {
    USING_PRE_FINALIZER(Wrapper, Dispose);
    USING_GARBAGE_COLLECTED_MIXIN(Wrapper);

   public:
    explicit Wrapper(ContextLifecycleNotifier* notifier) {
      SetContextLifecycleNotifier(notifier);
    }

    void Trace(Visitor* visitor) override {
      ContextLifecycleObserver::Trace(visitor);
    }

    void Dispose() { receiver_set_.Clear(); }

    mojo::ReceiverSet<Interface>& receiver_set() { return receiver_set_; }

    // ContextLifecycleObserver methods
    void ContextDestroyed() override {
      if (Mode == HeapMojoWrapperMode::kWithContextObserver)
        receiver_set_.Clear();
    }

   private:
    mojo::ReceiverSet<Interface> receiver_set_;
  };

  Member<Wrapper> wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_RECEIVER_SET_H_
