// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_HTTP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_HTTP_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

// WebUIMessageHandler for HTTP Messages to pass messages to the
// chrome://nearby-internals HTTP tab.
class NearbyInternalsHttpHandler : public content::WebUIMessageHandler {
 public:
  NearbyInternalsHttpHandler();
  NearbyInternalsHttpHandler(const NearbyInternalsHttpHandler&) = delete;
  NearbyInternalsHttpHandler& operator=(const NearbyInternalsHttpHandler&) =
      delete;
  ~NearbyInternalsHttpHandler() override;

  // content::WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Message handler callback that initializes JavaScript.
  void InitializeContents(const base::ListValue* args);

  // Message handler callback that calls Update Device RPC.
  void UpdateDevice(const base::ListValue* args);

  // Message handler callback that calls List Public Certificates RPC.
  void ListPublicCertificates(const base::ListValue* args);

  // Message handler callback that calls List Contacts RPC.
  void ListContactPeople(const base::ListValue* args);

  base::WeakPtrFactory<NearbyInternalsHttpHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_HTTP_HANDLER_H_
