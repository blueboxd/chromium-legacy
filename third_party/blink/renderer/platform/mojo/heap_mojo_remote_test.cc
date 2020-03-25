
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/receiver.h"
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

template <HeapMojoWrapperMode Mode>
class RemoteOwner : public GarbageCollected<RemoteOwner<Mode>> {
 public:
  explicit RemoteOwner(MockContext* context) : remote_(context) {}

  HeapMojoRemote<sample::blink::Service, Mode>& remote() { return remote_; }

  void Trace(Visitor* visitor) { visitor->Trace(remote_); }

  HeapMojoRemote<sample::blink::Service, Mode> remote_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoRemoteGCBaseTest : public TestSupportingGC {
 public:
  base::RunLoop& run_loop() { return run_loop_; }
  bool& disconnected() { return disconnected_; }

  void ClearOwner() { owner_ = nullptr; }

 protected:
  void SetUp() override {
    CHECK(!disconnected_);
    context_ = MakeGarbageCollected<MockContext>();
    owner_ = MakeGarbageCollected<RemoteOwner<Mode>>(context_);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    impl_.receiver().Bind(
        owner_->remote().BindNewPipeAndPassReceiver(null_task_runner));
    impl_.receiver().set_disconnect_handler(WTF::Bind(
        [](HeapMojoRemoteGCBaseTest* remote_test) {
          remote_test->run_loop().Quit();
          remote_test->disconnected() = true;
        },
        WTF::Unretained(this)));
  }

  ServiceImpl impl_;
  Persistent<MockContext> context_;
  Persistent<RemoteOwner<Mode>> owner_;
  base::RunLoop run_loop_;
  bool disconnected_ = false;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoRemoteDestroyContextBaseTest : public TestSupportingGC {
 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<MockContext>();
    owner_ = MakeGarbageCollected<RemoteOwner<Mode>>(context_);
    scoped_refptr<base::NullTaskRunner> null_task_runner =
        base::MakeRefCounted<base::NullTaskRunner>();
    impl_.receiver().Bind(
        owner_->remote().BindNewPipeAndPassReceiver(null_task_runner));
  }

  ServiceImpl impl_;
  Persistent<MockContext> context_;
  Persistent<RemoteOwner<Mode>> owner_;
};

}  // namespace

class HeapMojoRemoteGCWithContextObserverTest
    : public HeapMojoRemoteGCBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoRemoteGCWithoutContextObserverTest
    : public HeapMojoRemoteGCBaseTest<
          HeapMojoWrapperMode::kWithoutContextObserver> {};
class HeapMojoRemoteDestroyContextWithContextObserverTest
    : public HeapMojoRemoteDestroyContextBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoRemoteDestroyContextWithoutContextObserverTest
    : public HeapMojoRemoteDestroyContextBaseTest<
          HeapMojoWrapperMode::kWithoutContextObserver> {};

// Make HeapMojoRemote with context observer garbage collected and check that
// the connection is disconnected right after the marking phase.
TEST_F(HeapMojoRemoteGCWithContextObserverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
  CompleteSweepingIfNeeded();
}

// Make HeapMojoRemote without context observer garbage collected and check that
// the connection is disconnected right after the marking phase.
TEST_F(HeapMojoRemoteGCWithoutContextObserverTest, ResetsOnGC) {
  ClearOwner();
  EXPECT_FALSE(disconnected());
  PreciselyCollectGarbage();
  run_loop().Run();
  EXPECT_TRUE(disconnected());
  CompleteSweepingIfNeeded();
}

// Destroy the context with context observer and check that the connection is
// disconnected.
TEST_F(HeapMojoRemoteDestroyContextWithContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->remote().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_FALSE(owner_->remote().is_bound());
}

// Destroy the context without context observer and check that the connection is
// still connected.
TEST_F(HeapMojoRemoteDestroyContextWithoutContextObserverTest,
       ResetsOnContextDestroyed) {
  EXPECT_TRUE(owner_->remote().is_bound());
  context_->NotifyContextDestroyed();
  EXPECT_TRUE(owner_->remote().is_bound());
}

}  // namespace blink
