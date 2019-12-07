// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMPONENTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_COMPONENTS_HANDLER_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// The handler for Javascript messages for the chrome://components/ page.
class ComponentsHandler : public content::WebUIMessageHandler,
                          public component_updater::ServiceObserver {
 public:
  ComponentsHandler();
  ComponentsHandler(const ComponentsHandler&) = delete;
  ComponentsHandler& operator=(const ComponentsHandler&) = delete;
  ~ComponentsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Callback for the "requestComponentsData" message.
  void HandleRequestComponentsData(const base::ListValue* args);

  // Callback for the "checkUpdate" message.
  void HandleCheckUpdate(const base::ListValue* args);

  // ServiceObserver implementation.
  void OnEvent(Events event, const std::string& id) override;

 private:
  static base::string16 ComponentEventToString(Events event);
  static base::string16 ServiceStatusToString(
      update_client::ComponentState state);
  static std::unique_ptr<base::ListValue> LoadComponents();
  static void OnDemandUpdate(const std::string& component_id);

  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMPONENTS_HANDLER_H_
