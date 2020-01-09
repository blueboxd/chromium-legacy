// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#include <set>

#include "ios/chrome/browser/overlays/public/overlay_request_callback_installer.h"
#include "testing/gmock/include/gmock/gmock.h"

class OverlayResponseSupport;

// Interface for a test object whose interface is called by callbacks added
// by FakeOverlayRequestCallbackInstaller.
class FakeOverlayRequestCallbackReceiver {
 public:
  // Function used as the completion callback for |request|.  |response| is
  // |request|'s completion response.
  virtual void CompletionCallback(OverlayRequest* request,
                                  OverlayResponse* response) = 0;
  // Function used as the callback when |response| is dispatched through
  // |request|.  Only executed if |response| is supported by |response_support|.
  virtual void DispatchCallback(OverlayRequest* request,
                                const OverlayResponseSupport* response_support,
                                OverlayResponse* response) = 0;
};

// Mock version of the fake callback receiver receiver.
class MockOverlayRequestCallbackReceiver
    : public FakeOverlayRequestCallbackReceiver {
 public:
  MockOverlayRequestCallbackReceiver();
  ~MockOverlayRequestCallbackReceiver();

  MOCK_METHOD2(CompletionCallback,
               void(OverlayRequest* request, OverlayResponse* response));
  MOCK_METHOD3(DispatchCallback,
               void(OverlayRequest* request,
                    const OverlayResponseSupport* response_support,
                    OverlayResponse* response));
};

// OverlayRequestCallbackInstaller subclass used for testing.  Sets up callbacks
// that execute on a mocked receiver that is exposed for testing.
class FakeOverlayRequestCallbackInstaller
    : public OverlayRequestCallbackInstaller {
 public:
  // Constructor for a fake callback installer that creates callbacks that are
  // forwarded to |receiver|.  |receiver| must be non-null, and must outlive any
  // requests passed to InstallCallback().
  explicit FakeOverlayRequestCallbackInstaller(
      FakeOverlayRequestCallbackReceiver* receiver);
  ~FakeOverlayRequestCallbackInstaller() override;

  // Sets the request support for the callback installer.  |request_support|
  // must not be null.  All requests are supported by default.
  void SetRequestSupport(const OverlayRequestSupport* request_support);

  // Begins installing dispatch callbacks for OverlayRequests that are executed
  // for dispatched responses supported by |response_support|.  Installed
  // dispatch callbacks will execute MockCallbackReveiver::DispatchCallback()
  // with |response_support|.
  void StartInstallingDispatchCallbacksWithSupport(
      const OverlayResponseSupport* response_support);

 private:
  // OverlayRequestCallbackInstaller:
  const OverlayRequestSupport* GetRequestSupport() const override;
  void InstallCallbacksInternal(OverlayRequest* request) override;

  FakeOverlayRequestCallbackReceiver* receiver_ = nullptr;
  const OverlayRequestSupport* request_support_ = nullptr;
  std::set<const OverlayResponseSupport*> dispatch_response_supports_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
