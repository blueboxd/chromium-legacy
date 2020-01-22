// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLINK_TEST_CLIENT_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLINK_TEST_CLIENT_H_

#include "base/macros.h"
#include "content/shell/common/web_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"

namespace content {

class WebTestBlinkTestClient : public mojom::WebTestClient {
 public:
  WebTestBlinkTestClient() = default;
  ~WebTestBlinkTestClient() override = default;

  static void Create(mojo::PendingReceiver<mojom::WebTestClient> receiver);

 private:
  // WebTestClient implementation.
  void InitiateLayoutDump() override;
  void PrintMessageToStderr(const std::string& message) override;
  void Reload() override;
  void CloseRemainingWindows() override;
  void GoToOffset(int offset) override;
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument) override;
  void SetBluetoothManualChooser(bool enable) override;
  void GetBluetoothManualChooserEvents() override;
  void SetPopupBlockingEnabled(bool block_popups) override;
  void LoadURLForFrame(const GURL& url, const std::string& frame_name) override;
  void NavigateSecondaryWindow(const GURL& url) override;

  DISALLOW_COPY_AND_ASSIGN(WebTestBlinkTestClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLINK_TEST_CLIENT_H_
