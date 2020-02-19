
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap_observer_set.h"

namespace blink {

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
  HeapObserverSet<ContextLifecycleObserver> observers_;
};

class ReceiverOwner : public GarbageCollected<ReceiverOwner>,
                      public sample::blink::Service {
 public:
  explicit ReceiverOwner(MockContext* context) : receiver_(this, context) {}

  HeapMojoReceiver<sample::blink::Service>& receiver() { return receiver_; }

  void Trace(Visitor* visitor) { visitor->Trace(receiver_); }

 private:
  // sample::blink::Service implementation
  void Frobinate(sample::blink::FooPtr foo,
                 sample::blink::Service::BazOptions options,
                 mojo::PendingRemote<sample::blink::Port> port,
                 sample::blink::Service::FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> port) override {}

  HeapMojoReceiver<sample::blink::Service> receiver_;
};

class HeapMojoReceiverTest : public TestSupportingGC {
 public:
  base::RunLoop& run_loop() { return run_loop_; }
  bool& disconnected() { return disconnected_; }

  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    CHECK(!disconnected_);
    context_ = MakeGarbageCollected<MockContext>();
    owner_ = MakeGarbageCollected<ReceiverOwner>(context_);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    remote_ = mojo::Remote<sample::blink::Service>(
        owner_->receiver().BindNewPipeAndPassRemote(null_task_runner));
    remote_.set_disconnect_handler(WTF::Bind(
        [](HeapMojoReceiverTest* receiver_test) {
          receiver_test->run_loop().Quit();
          receiver_test->disconnected() = true;
        },
        WTF::Unretained(this)));
  }
  void TearDown() override { CHECK(disconnected_); }

  Persistent<MockContext> context_;
  Persistent<ReceiverOwner> owner_;
  base::RunLoop run_loop_;
  mojo::Remote<sample::blink::Service> remote_;
  bool disconnected_ = false;
};

// Make HeapMojoReceiver garbage collected and check that the connection is
// disconnected right after the marking phase.
TEST_F(HeapMojoReceiverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage(BlinkGC::kConcurrentAndLazySweeping);
  run_loop().Run();
  EXPECT_TRUE(disconnected());
  CompleteSweepingIfNeeded();
}

// Destroy the context and check that the connection is disconnected.
TEST_F(HeapMojoReceiverTest, ResetsOnContextDestroyed) {
  EXPECT_FALSE(disconnected());
  context_->NotifyContextDestroyed();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
}

}  // namespace blink
