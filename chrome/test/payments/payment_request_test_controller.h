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
#else
namespace content {
class WebContents;
}
#endif

namespace payments {

// Observe states or actions taken by the PaymentRequest in tests, supporting
// both Android and desktop.
class PaymentRequestTestObserver {
 public:
  virtual void OnCanMakePaymentCalled() {}
  virtual void OnCanMakePaymentReturned() {}
  virtual void OnHasEnrolledInstrumentCalled() {}
  virtual void OnHasEnrolledInstrumentReturned() {}
  virtual void OnShowAppsReady() {}
  virtual void OnNotSupportedError() {}
  virtual void OnConnectionTerminated() {}
  virtual void OnAbortCalled() {}
  virtual void OnCompleteCalled() {}

 protected:
  virtual ~PaymentRequestTestObserver() {}
};

// A class to control creation and behaviour of PaymentRequests in a
// cross-platform way for testing both Android and desktop.
class PaymentRequestTestController {
 public:
  PaymentRequestTestController();
  ~PaymentRequestTestController();

  // To be called from an override of BrowserTestBase::SetUpOnMainThread().
  void SetUpOnMainThread();

  void SetObserver(PaymentRequestTestObserver* observer);

  // Sets values that will change the behaviour of PaymentRequests created in
  // the future.
  void SetIncognito(bool is_incognito);
  void SetValidSsl(bool valid_ssl);
  void SetCanMakePaymentEnabledPref(bool can_make_payment_enabled);
#if defined(OS_ANDROID)
  // Get the WebContents of the Expandable Payment Handler for testing purpose,
  // or null if nonexistent. To guarantee a non-null return, this function
  // should be called only if: 1) PaymentRequest UI is opening. 2)
  // ScrollToExpandPaymentHandler feature is enabled. 3) PaymentHandler is
  // opening.
  content::WebContents* GetPaymentHandlerWebContents();
#endif

 private:
  // Observers that forward through to the PaymentRequestTestObserver.
  void OnCanMakePaymentCalled();
  void OnCanMakePaymentReturned();
  void OnHasEnrolledInstrumentCalled();
  void OnHasEnrolledInstrumentReturned();
  void OnShowAppsReady();
  void OnNotSupportedError();
  void OnConnectionTerminated();
  void OnAbortCalled();
  void OnCompleteCalled();

  PaymentRequestTestObserver* observer_ = nullptr;

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
