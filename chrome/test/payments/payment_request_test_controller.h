// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PAYMENTS_PAYMENT_REQUEST_TEST_CONTROLLER_H_
#define CHROME_TEST_PAYMENTS_PAYMENT_REQUEST_TEST_CONTROLLER_H_

#include <memory>

#include "build/build_config.h"

#if !defined(OS_ANDROID)
namespace sync_preferences {
class TestingPrefServiceSyncable;
}
#endif

namespace payments {

// Observe states or actions taken by the PaymentRequest in tests, supporting
// both Android and desktop.
class PaymentRequestTestObserver {
 public:
  virtual void OnCanMakePaymentCalled() = 0;
  virtual void OnCanMakePaymentReturned() = 0;
  virtual void OnHasEnrolledInstrumentCalled() = 0;
  virtual void OnHasEnrolledInstrumentReturned() = 0;
  virtual void OnNotSupportedError() = 0;
  virtual void OnConnectionTerminated() = 0;
  virtual void OnAbortCalled() = 0;

 protected:
  virtual ~PaymentRequestTestObserver() {}
};

// A class to control creation and behaviour of PaymentRequests in a
// cross-platform way for testing both Android and desktop.
class PaymentRequestTestController {
 public:
  explicit PaymentRequestTestController(PaymentRequestTestObserver* observer);
  ~PaymentRequestTestController();

  // To be called from an override of BrowserTestBase::SetUpOnMainThread().
  void SetUpOnMainThread();

  // Sets values that will change the behaviour of PaymentRequests created in
  // the future.
  void SetIncognito(bool is_incognito);
  void SetValidSsl(bool valid_ssl);
  void SetCanMakePaymentEnabledPref(bool can_make_payment_enabled);

 private:
  // Observers that forward through to the PaymentRequestTestObserver.
  void OnCanMakePaymentCalled();
  void OnCanMakePaymentReturned();
  void OnHasEnrolledInstrumentCalled();
  void OnHasEnrolledInstrumentReturned();
  void OnNotSupportedError();
  void OnConnectionTerminated();
  void OnAbortCalled();

  PaymentRequestTestObserver* const observer_;

  bool is_incognito_ = false;
  bool valid_ssl_ = true;
  bool can_make_payment_pref_ = true;

#if !defined(OS_ANDROID)
  void UpdateDelegateFactory();

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;

  class ObserverConverter;
  std::unique_ptr<ObserverConverter> observer_converter_;
#endif
};

}  // namespace payments

#endif  // CHROME_TEST_PAYMENTS_PAYMENT_REQUEST_TEST_CONTROLLER_H_
