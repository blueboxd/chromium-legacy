// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BULK_LEAK_CHECK_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BULK_LEAK_CHECK_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace password_manager {

class BulkLeakCheck;

// The service that allows to check arbitrary number of passwords against the
// database of leaked credentials.
class BulkLeakCheckService : public KeyedService,
                             public BulkLeakCheckDelegateInterface {
 public:
  enum class State {
    // The service is idle and there was no previous error.
    kIdle,
    // The service is checking some credentials.
    kRunning,

    // Those below are error states. On any error the current job is aborted.
    // The error is sticky until next CheckUsernamePasswordPairs() call.

    // Cancel() aborted the running check.
    kCanceled,
    // The user isn't signed-in to Chrome.
    kSignedOut,
    // Error obtaining an access token.
    kTokenRequestFailure,
    // Error in hashing/encrypting for the request.
    kHashingFailure,
    // Error related to network.
    kNetworkError,
    // Error related to the password leak Google service.
    kServiceError,
    // Error related to the quota limit of the password leak Google service.
    kQuotaLimit,
  };

  class Observer : public base::CheckedObserver {
   public:
    // BulkLeakCheckService changed its state.
    virtual void OnStateChanged(State state) = 0;

    // Called when |credential| is analyzed.
    virtual void OnCredentialDone(const LeakCheckCredential& credential,
                                  IsLeaked is_leaked) = 0;
  };

  BulkLeakCheckService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~BulkLeakCheckService() override;

  // Starts the checks or appends |credentials| to the existing queue.
  void CheckUsernamePasswordPairs(std::vector<LeakCheckCredential> credentials);

  // Stops all the current checks immediately.
  void Cancel();

  // Returns number of pending passwords to be checked.
  size_t GetPendingChecksCount() const;

  // Returns the current state of the service.
  State state() const { return state_; }

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }

  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

  // KeyedService:
  void Shutdown() override;

#if defined(UNIT_TEST)
  void set_leak_factory(std::unique_ptr<LeakDetectionCheckFactory> factory) {
    leak_check_factory_ = std::move(factory);
  }

  void set_state_and_notify(State state) {
    state_ = state;
    NotifyStateChanged();
  }
#endif  // defined(UNIT_TEST)

 private:
  class MetricsReporter;
  // BulkLeakCheckDelegateInterface:
  void OnFinishedCredential(LeakCheckCredential credential,
                            IsLeaked is_leaked) override;
  void OnError(LeakDetectionError error) override;

  // Notify the observers.
  void NotifyStateChanged();

  signin::IdentityManager* identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Factory to create |bulk_leak_check_|.
  std::unique_ptr<LeakDetectionCheckFactory> leak_check_factory_;
  // Currently running check.
  std::unique_ptr<BulkLeakCheck> bulk_leak_check_;
  // Reports metrics about bulk leak check.
  std::unique_ptr<MetricsReporter> metrics_reporter_;

  State state_ = State::kIdle;
  base::ObserverList<Observer> observers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BULK_LEAK_CHECK_SERVICE_H_
