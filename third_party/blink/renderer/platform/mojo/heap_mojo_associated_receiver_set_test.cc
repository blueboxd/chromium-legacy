// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver_set.h"

#include <utility>

#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap_observer_list.h"

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

class GCOwner;

class HeapMojoAssociatedReceiverSetGCBaseTest : public TestSupportingGC {
 public:
  FakeContextNotifier* context() { return context_; }
  scoped_refptr<base::NullTaskRunner> task_runner() {
    return null_task_runner_;
  }
  GCOwner* owner() { return owner_; }
  void set_is_owner_alive(bool alive) { is_owner_alive_ = alive; }

  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<FakeContextNotifier>();
    owner_ = MakeGarbageCollected<GCOwner>(context(), this);
  }
  void TearDown() override {
    owner_ = nullptr;
    PreciselyCollectGarbage();
  }

  Persistent<FakeContextNotifier> context_;
  Persistent<GCOwner> owner_;
  bool is_owner_alive_ = false;
  scoped_refptr<base::NullTaskRunner> null_task_runner_ =
      base::MakeRefCounted<base::NullTaskRunner>();
};

class GCOwner : public GarbageCollected<GCOwner>,
                public sample::blink::Service {
 public:
  explicit GCOwner(FakeContextNotifier* context,
                   HeapMojoAssociatedReceiverSetGCBaseTest* test)
      : associated_receiver_set_(this, context), test_(test) {
    test_->set_is_owner_alive(true);
  }
  void Dispose() { test_->set_is_owner_alive(false); }
  void Trace(Visitor* visitor) { visitor->Trace(associated_receiver_set_); }

  HeapMojoAssociatedReceiverSet<sample::blink::Service, GCOwner>&
  associated_receiver_set() {
    return associated_receiver_set_;
  }

  void Frobinate(sample::blink::FooPtr foo,
                 Service::BazOptions baz,
                 mojo::PendingRemote<sample::blink::Port> port,
                 FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> receiver) override {}

 private:
  HeapMojoAssociatedReceiverSet<sample::blink::Service, GCOwner>
      associated_receiver_set_;
  HeapMojoAssociatedReceiverSetGCBaseTest* test_;
};

}  // namespace

// Remove() a PendingAssociatedReceiver from HeapMojoAssociatedReceiverSet and
// verify that the receiver is no longer part of the set.
TEST_F(HeapMojoAssociatedReceiverSetGCBaseTest, RemovesReceiver) {
  auto& associated_receiver_set = owner()->associated_receiver_set();
  mojo::AssociatedRemote<sample::blink::Service> remote;
  auto receiver = remote.BindNewEndpointAndPassDedicatedReceiverForTesting();

  mojo::ReceiverId rid =
      associated_receiver_set.Add(std::move(receiver), task_runner());
  EXPECT_TRUE(associated_receiver_set.HasReceiver(rid));

  associated_receiver_set.Remove(rid);

  EXPECT_FALSE(associated_receiver_set.HasReceiver(rid));
}

// Clear() a HeapMojoAssociatedReceiverSet and verify that it is empty.
TEST_F(HeapMojoAssociatedReceiverSetGCBaseTest, ClearLeavesSetEmpty) {
  auto& associated_receiver_set = owner()->associated_receiver_set();
  mojo::AssociatedRemote<sample::blink::Service> remote;
  auto receiver = remote.BindNewEndpointAndPassDedicatedReceiverForTesting();

  mojo::ReceiverId rid =
      associated_receiver_set.Add(std::move(receiver), task_runner());
  EXPECT_TRUE(associated_receiver_set.HasReceiver(rid));

  associated_receiver_set.Clear();

  EXPECT_FALSE(associated_receiver_set.HasReceiver(rid));
}

}  // namespace blink
