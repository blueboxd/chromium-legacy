// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_REMOTE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_REMOTE_H_

#include <utility>

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

// HeapMojoRemote is a wrapper for mojo::Remote to be owned by a
// garbage-collected object. Blink is expected to use HeapMojoRemote by
// default. HeapMojoRemote must be associated with context.
// HeapMojoRemote's constructor takes context as a mandatory parameter.
// HeapMojoRemote resets the mojo connection when 1) the owner object is
// garbage-collected and 2) the associated ExecutionContext is detached.

// TODO(crbug.com/1058076) HeapMojoWrapperMode should be removed once we ensure
// that the interface is not used after ContextDestroyed().
template <typename Interface,
          HeapMojoWrapperMode Mode = HeapMojoWrapperMode::kWithContextObserver>
class HeapMojoRemote {
  DISALLOW_NEW();

 public:
  explicit HeapMojoRemote(ContextLifecycleNotifier* notifier)
      : wrapper_(MakeGarbageCollected<Wrapper>(notifier)) {}

  // Methods to redirect to mojo::Remote.
  using Proxy = typename Interface::Proxy_;
  Proxy* operator->() const { return get(); }
  Proxy* get() const { return wrapper_->remote().get(); }
  bool is_bound() const { return wrapper_->remote().is_bound(); }
  bool is_connected() const { return wrapper_->remote().is_connected(); }
  void reset() { wrapper_->remote().reset(); }
  void set_disconnect_handler(base::OnceClosure handler) {
    wrapper_->remote().set_disconnect_handler(std::move(handler));
  }
  mojo::PendingReceiver<Interface> BindNewPipeAndPassReceiver(
      scoped_refptr<base::SequencedTaskRunner> task_runner) WARN_UNUSED_RESULT {
    DCHECK(task_runner);
    return wrapper_->remote().BindNewPipeAndPassReceiver(
        std::move(task_runner));
  }
  void Bind(mojo::PendingRemote<Interface> pending_remote,
            scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(task_runner);
    wrapper_->remote().Bind(std::move(pending_remote), std::move(task_runner));
  }
  void FlushForTesting() { return wrapper_->remote().FlushForTesting(); }

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

    void Dispose() { remote_.reset(); }

    mojo::Remote<Interface>& remote() { return remote_; }

    // ContextLifecycleObserver methods
    void ContextDestroyed() override {
      if (Mode == HeapMojoWrapperMode::kWithContextObserver)
        remote_.reset();
    }

   private:
    mojo::Remote<Interface> remote_;
  };

  Member<Wrapper> wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_REMOTE_H_
