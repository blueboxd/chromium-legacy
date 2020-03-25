
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "base/test/null_task_runner.h"
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

class MockContext final : public GarbageCollected<MockContext>,
                          public ContextLifecycleNotifier {
  USING_GARBAGE_COLLECTED_MIXIN(MockContext);

 public:
  MockContext() = default;

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

template <HeapMojoWrapperMode Mode>
class ReceiverOwner : public GarbageCollected<ReceiverOwner<Mode>>,
                      public sample::blink::Service {
 public:
  explicit ReceiverOwner(MockContext* context) : receiver_(this, context) {}

  HeapMojoReceiver<sample::blink::Service, Mode>& receiver() {
    return receiver_;
  }

  void Trace(Visitor* visitor) { visitor->Trace(receiver_); }

 private:
  // sample::blink::Service implementation
  void Frobinate(sample::blink::FooPtr foo,
                 sample::blink::Service::BazOptions options,
                 mojo::PendingRemote<sample::blink::Port> port,
                 sample::blink::Service::FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> port) override {}

  HeapMojoReceiver<sample::blink::Service, Mode> receiver_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoReceiverGCBaseTest : public TestSupportingGC {
 public:
  base::RunLoop& run_loop() { return run_loop_; }
  bool& disconnected() { return disconnected_; }

  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    CHECK(!disconnected_);
    context_ = MakeGarbageCollected<MockContext>();
    owner_ = MakeGarbageCollected<ReceiverOwner<Mode>>(context_);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    remote_ = mojo::Remote<sample::blink::Service>(
        owner_->receiver().BindNewPipeAndPassRemote(null_task_runner));
    remote_.set_disconnect_handler(WTF::Bind(
        [](HeapMojoReceiverGCBaseTest* receiver_test) {
          receiver_test->run_loop().Quit();
          receiver_test->disconnected() = true;
        },
        WTF::Unretained(this)));
  }
  void TearDown() override { CHECK(disconnected_); }

  Persistent<MockContext> context_;
  Persistent<ReceiverOwner<Mode>> owner_;
  base::RunLoop run_loop_;
  mojo::Remote<sample::blink::Service> remote_;
  bool disconnected_ = false;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoReceiverDestroyContextBaseTest : public TestSupportingGC {
 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<MockContext>();
    owner_ = MakeGarbageCollected<ReceiverOwner<Mode>>(context_);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    remote_ = mojo::Remote<sample::blink::Service>(
        owner_->receiver().BindNewPipeAndPassRemote(null_task_runner));
  }

  Persistent<MockContext> context_;
  Persistent<ReceiverOwner<Mode>> owner_;
  mojo::Remote<sample::blink::Service> remote_;
};

}  // namespace

class HeapMojoReceiverGCWithContextObserverTest
    : public HeapMojoReceiverGCBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoReceiverGCWithoutContextObserverTest
    : public HeapMojoReceiverGCBaseTest<
          HeapMojoWrapperMode::kWithoutContextObserver> {};
class HeapMojoReceiverDestroyContextWithContextObserverTest
    : public HeapMojoReceiverDestroyContextBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoReceiverDestroyContextWithoutContextObserverTest
    : public HeapMojoReceiverDestroyContextBaseTest<
          HeapMojoWrapperMode::kWithoutContextObserver> {};

// Make HeapMojoReceiver with context observer garbage collected and check that
// the connection is disconnected right after the marking phase.
TEST_F(HeapMojoReceiverGCWithContextObserverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
  CompleteSweepingIfNeeded();
}

// Make HeapMojoReceiver without context observer garbage collected and check
// that the connection is disconnected right after the marking phase.
TEST_F(HeapMojoReceiverGCWithoutContextObserverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
  CompleteSweepingIfNeeded();
}

// Destroy the context with context observer and check that the connection is
// disconnected.
TEST_F(HeapMojoReceiverDestroyContextWithContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->receiver().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_FALSE(owner_->receiver().is_bound());
}

// Destroy the context without context observer and check that the connection is
// still connected.
TEST_F(HeapMojoReceiverDestroyContextWithoutContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->receiver().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_TRUE(owner_->receiver().is_bound());
}

}  // namespace blink
