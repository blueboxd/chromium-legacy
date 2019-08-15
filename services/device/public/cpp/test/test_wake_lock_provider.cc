// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/test_wake_lock_provider.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/public/mojom/constants.mojom.h"

namespace device {

// TestWakeLock implements mojom::WakeLock on behalf of TestWakeLockProvider.
class TestWakeLockProvider::TestWakeLock : public mojom::WakeLock,
                                           public service_manager::Service {
 public:
  TestWakeLock(mojom::WakeLockRequest request,
               mojom::WakeLockType type,
               TestWakeLockProvider* provider)
      : type_(type), provider_(provider) {
    AddClient(std::move(request));
    bindings_.set_connection_error_handler(base::BindRepeating(
        &TestWakeLock::OnConnectionError, base::Unretained(this)));
  }

  ~TestWakeLock() override = default;

  mojom::WakeLockType type() const { return type_; }

  // mojom::WakeLock:
  void RequestWakeLock() override {
    DCHECK(bindings_.dispatch_context());
    DCHECK_GE(num_lock_requests_, 0);

    // Coalesce consecutive requests from the same client.
    if (*bindings_.dispatch_context())
      return;

    *bindings_.dispatch_context() = true;
    num_lock_requests_++;
    CheckAndNotifyProvider();
  }

  void CancelWakeLock() override {
    DCHECK(bindings_.dispatch_context());

    // Coalesce consecutive cancel requests from the same client. Also ignore a
    // CancelWakeLock call without a RequestWakeLock call.
    if (!(*bindings_.dispatch_context()))
      return;

    DCHECK_GT(num_lock_requests_, 0);
    *bindings_.dispatch_context() = false;
    num_lock_requests_--;
    CheckAndNotifyProvider();
  }

  void AddClient(mojom::WakeLockRequest request) override {
    bindings_.AddBinding(this, std::move(request),
                         std::make_unique<bool>(false));
  }

  void ChangeType(mojom::WakeLockType type,
                  ChangeTypeCallback callback) override {
    NOTIMPLEMENTED();
  }

  void HasWakeLockForTests(HasWakeLockForTestsCallback callback) override {
    NOTIMPLEMENTED();
  }

  void OnConnectionError() {
    // If there is an outstanding request by this client then decrement its
    // request and check if the wake lock is deactivated.
    DCHECK(bindings_.dispatch_context());
    if (*bindings_.dispatch_context() && num_lock_requests_ > 0) {
      num_lock_requests_--;
      CheckAndNotifyProvider();
    }

    // TestWakeLockProvider will take care of deleting this object as it owns
    // it.
    if (bindings_.empty())
      provider_->OnConnectionError(type_, this);
  }

 private:
  void CheckAndNotifyProvider() {
    if (num_lock_requests_ == 1) {
      provider_->OnWakeLockActivated(type_);
      return;
    }

    if (num_lock_requests_ == 0) {
      provider_->OnWakeLockDeactivated(type_);
      return;
    }
  }

  mojom::WakeLockType type_;

  // Not owned.
  TestWakeLockProvider* provider_;

  mojo::BindingSet<mojom::WakeLock, std::unique_ptr<bool>> bindings_;

  int num_lock_requests_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestWakeLock);
};

// Holds the state associated with wake locks of a single type across the
// system i.e. if 3 |kAppSuspension| wake locks are currently held the |count|
// would be 3.
struct TestWakeLockProvider::WakeLockDataPerType {
  WakeLockDataPerType() = default;
  WakeLockDataPerType(WakeLockDataPerType&&) = default;
  ~WakeLockDataPerType() = default;

  // Currently held count of this wake lock type.
  int64_t count = 0;

  // Map of all wake locks of this type created by this provider. An entry is
  // removed from this map when an |OnConnectionError| is received.
  std::map<TestWakeLock*, std::unique_ptr<TestWakeLock>> wake_locks;

  // Observers for this wake lock type.
  mojo::InterfacePtrSet<mojom::WakeLockObserver> observers;

  DISALLOW_COPY_AND_ASSIGN(WakeLockDataPerType);
};

TestWakeLockProvider::TestWakeLockProvider() {
  // Populates |wake_lock_store_| with entries for all types of wake locks.
  wake_lock_store_[mojom::WakeLockType::kPreventAppSuspension] =
      std::make_unique<WakeLockDataPerType>();
  wake_lock_store_[mojom::WakeLockType::kPreventDisplaySleep] =
      std::make_unique<WakeLockDataPerType>();
  wake_lock_store_[mojom::WakeLockType::kPreventDisplaySleepAllowDimming] =
      std::make_unique<WakeLockDataPerType>();
}

TestWakeLockProvider::TestWakeLockProvider(
    service_manager::mojom::ServiceRequest request)
    : TestWakeLockProvider() {
  service_binding_.Bind(std::move(request));
}

TestWakeLockProvider::~TestWakeLockProvider() = default;

void TestWakeLockProvider::BindReceiver(
    mojo::PendingReceiver<mojom::WakeLockProvider> receiver) {
  bindings_.AddBinding(this, std::move(receiver));
}

void TestWakeLockProvider::GetWakeLockContextForID(
    int context_id,
    mojo::InterfaceRequest<mojom::WakeLockContext> request) {
  // This method is only used on Android.
  NOTIMPLEMENTED();
}

void TestWakeLockProvider::GetWakeLockWithoutContext(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    mojom::WakeLockRequest request) {
  // Create a wake lock and store it to manage it's lifetime.
  auto wake_lock =
      std::make_unique<TestWakeLock>(std::move(request), type, this);
  GetWakeLockDataPerType(type).wake_locks[wake_lock.get()] =
      std::move(wake_lock);
}

void TestWakeLockProvider::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK_EQ(interface_name, mojom::WakeLockProvider::Name_);
  bindings_.AddBinding(
      this, mojom::WakeLockProviderRequest(std::move(interface_pipe)));
}

void TestWakeLockProvider::OnConnectionError(mojom::WakeLockType type,
                                             TestWakeLock* wake_lock) {
  size_t result = GetWakeLockDataPerType(type).wake_locks.erase(wake_lock);
  DCHECK_GT(result, 0UL);
}

TestWakeLockProvider::WakeLockDataPerType&
TestWakeLockProvider::GetWakeLockDataPerType(mojom::WakeLockType type) const {
  auto it = wake_lock_store_.find(type);
  // An entry for |type| should always be created in the constructor.
  DCHECK(it != wake_lock_store_.end());
  return *(it->second);
}

void TestWakeLockProvider::OnWakeLockActivated(mojom::WakeLockType type) {
  // Increment the currently activated wake locks of type |type|.
  const int64_t old_count = GetWakeLockDataPerType(type).count;
  DCHECK_GE(old_count, 0);

  GetWakeLockDataPerType(type).count = old_count + 1;
}

void TestWakeLockProvider::OnWakeLockDeactivated(mojom::WakeLockType type) {
  // Decrement the currently activated wake locks of type |type|.
  const int64_t old_count = GetWakeLockDataPerType(type).count;
  DCHECK_GT(old_count, 0);

  const int64_t new_count = old_count - 1;
  GetWakeLockDataPerType(type).count = new_count;
  // Notify observers of the last cancelation i.e. deactivation of wake lock
  // type |type|.
  if (new_count == 0) {
    GetWakeLockDataPerType(type).observers.ForAllPtrs(
        [type](mojom::WakeLockObserver* wake_lock_observer) {
          wake_lock_observer->OnWakeLockDeactivated(type);
        });
  }
}

void TestWakeLockProvider::NotifyOnWakeLockDeactivation(
    mojom::WakeLockType type,
    mojom::WakeLockObserverPtr observer) {
  // Notify observer immediately if wake lock is deactivated. Add it to the
  // observers list for future deactivation notifications.
  if (GetWakeLockDataPerType(type).count == 0) {
    observer->OnWakeLockDeactivated(type);
  }
  GetWakeLockDataPerType(type).observers.AddPtr(std::move(observer));
}

void TestWakeLockProvider::GetActiveWakeLocksForTests(
    mojom::WakeLockType type,
    GetActiveWakeLocksForTestsCallback callback) {
  std::move(callback).Run(GetWakeLockDataPerType(type).count);
}

}  // namespace device
