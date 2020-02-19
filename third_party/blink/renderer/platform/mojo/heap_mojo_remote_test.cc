
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/receiver.h"
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

class ServiceImpl : public sample::blink::Service {
 public:
  explicit ServiceImpl() = default;

  mojo::Receiver<sample::blink::Service>& receiver() { return receiver_; }

 private:
  // sample::blink::Service implementation
  void Frobinate(sample::blink::FooPtr foo,
                 sample::blink::Service::BazOptions options,
                 mojo::PendingRemote<sample::blink::Port> port,
                 sample::blink::Service::FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> port) override {}

  mojo::Receiver<sample::blink::Service> receiver_{this};
};

class RemoteOwner : public GarbageCollected<RemoteOwner> {
 public:
  explicit RemoteOwner(MockContext* context) : remote_(context) {}

  HeapMojoRemote<sample::blink::Service>& remote() { return remote_; }

  void Trace(Visitor* visitor) { visitor->Trace(remote_); }

  HeapMojoRemote<sample::blink::Service> remote_;
};

class HeapMojoRemoteTest : public TestSupportingGC {
 public:
  base::RunLoop& run_loop() { return run_loop_; }
  bool& disconnected() { return disconnected_; }

  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    CHECK(!disconnected_);
    context_ = MakeGarbageCollected<MockContext>();
    owner_ = MakeGarbageCollected<RemoteOwner>(context_);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    impl_.receiver().Bind(
        owner_->remote().BindNewPipeAndPassReceiver(null_task_runner));
    impl_.receiver().set_disconnect_handler(WTF::Bind(
        [](HeapMojoRemoteTest* remote_test) {
          remote_test->run_loop().Quit();
          remote_test->disconnected() = true;
        },
        WTF::Unretained(this)));
  }
  void TearDown() override {
    // CHECK(disconnected_);
  }

  ServiceImpl impl_;
  Persistent<MockContext> context_;
  Persistent<RemoteOwner> owner_;
  base::RunLoop run_loop_;
  bool disconnected_ = false;
};

// Make HeapMojoRemote garbage collected and check that the connection is
// disconnected right after the marking phase.
TEST_F(HeapMojoRemoteTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage(BlinkGC::kConcurrentAndLazySweeping);
  run_loop().Run();
  EXPECT_TRUE(disconnected());
  CompleteSweepingIfNeeded();
}

// Destroy the context and check that the connection is disconnected.
TEST_F(HeapMojoRemoteTest, ResetsOnContextDestroyed) {
  EXPECT_FALSE(disconnected());
  context_->NotifyContextDestroyed();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
}

}  // namespace blink
