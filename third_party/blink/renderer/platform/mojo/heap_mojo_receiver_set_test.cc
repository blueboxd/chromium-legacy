// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"

#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap_observer_list.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

namespace {

class FakeContextNotifier final : public GarbageCollected<FakeContextNotifier>,
                                  public ContextLifecycleNotifier {
  USING_GARBAGE_COLLECTED_MIXIN(FakeContextNotifier);

 public:
  FakeContextNotifier() = default;

  void AddContextLifecycleObserver(
      ContextLifecycleObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveContextLifecycleObserver(
      ContextLifecycleObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyContextDestroyed() {
    observers_.ForEachObserver([](ContextLifecycleObserver* observer) {
      observer->ContextDestroyed();
    });
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(observers_);
    ContextLifecycleNotifier::Trace(visitor);
  }

 private:
  HeapObserverList<ContextLifecycleObserver> observers_;
};

class MockService : public sample::blink::Service {
 public:
  MockService() = default;

  void Frobinate(sample::blink::FooPtr foo,
                 Service::BazOptions baz,
                 mojo::PendingRemote<sample::blink::Port> port,
                 FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> receiver) override {}
};

template <HeapMojoWrapperMode Mode>
class GCOwner : public GarbageCollected<GCOwner<Mode>> {
 public:
  explicit GCOwner(FakeContextNotifier* context) : receiver_set_(context) {}
  void Trace(Visitor* visitor) { visitor->Trace(receiver_set_); }

  HeapMojoReceiverSet<sample::blink::Service, Mode>& receiver_set() {
    return receiver_set_;
  }

 private:
  HeapMojoReceiverSet<sample::blink::Service, Mode> receiver_set_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoReceiverSetGCBaseTest : public TestSupportingGC {
 public:
  FakeContextNotifier* context() { return context_; }
  scoped_refptr<base::NullTaskRunner> task_runner() {
    return null_task_runner_;
  }
  GCOwner<Mode>* owner() { return owner_; }

  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<FakeContextNotifier>();
    owner_ = MakeGarbageCollected<GCOwner<Mode>>(context());
  }
  void TearDown() override {}

  Persistent<FakeContextNotifier> context_;
  Persistent<GCOwner<Mode>> owner_;
  scoped_refptr<base::NullTaskRunner> null_task_runner_ =
      base::MakeRefCounted<base::NullTaskRunner>();
};

}  // namespace

class HeapMojoReceiverSetGCWithContextObserverTest
    : public HeapMojoReceiverSetGCBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoReceiverSetGCWithoutContextObserverTest
    : public HeapMojoReceiverSetGCBaseTest<
          HeapMojoWrapperMode::kWithoutContextObserver> {};

// GC the HeapMojoReceiverSet with context observer and verify that the receiver
// is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoReceiverSetGCWithContextObserverTest, RemovesReceiver) {
  auto receiver_set = owner()->receiver_set();
  MockService service;
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid =
      receiver_set.Add(&service, std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));

  receiver_set.Remove(rid);

  EXPECT_FALSE(receiver_set.HasReceiver(rid));
}

// GC the HeapMojoReceiverSet without context observer and verify that the
// receiver is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoReceiverSetGCWithoutContextObserverTest, RemovesReceiver) {
  auto receiver_set = owner()->receiver_set();
  MockService service;
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid =
      receiver_set.Add(&service, std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));

  receiver_set.Remove(rid);

  EXPECT_FALSE(receiver_set.HasReceiver(rid));
}

// GC the HeapMojoReceiverSet with context observer and verify that the receiver
// is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoReceiverSetGCWithContextObserverTest, ClearLeavesSetEmpty) {
  auto receiver_set = owner()->receiver_set();
  MockService service;
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid =
      receiver_set.Add(&service, std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));

  receiver_set.Clear();

  EXPECT_FALSE(receiver_set.HasReceiver(rid));
}

// GC the HeapMojoReceiverSet without context observer and verify that the
// receiver is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoReceiverSetGCWithoutContextObserverTest, ClearLeavesSetEmpty) {
  auto receiver_set = owner()->receiver_set();
  MockService service;
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid =
      receiver_set.Add(&service, std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));

  receiver_set.Clear();

  EXPECT_FALSE(receiver_set.HasReceiver(rid));
}

}  // namespace blink
