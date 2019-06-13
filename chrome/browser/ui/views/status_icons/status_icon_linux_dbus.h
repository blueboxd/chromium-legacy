// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_DBUS_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_DBUS_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "ui/views/linux_ui/status_icon_linux.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

class DbusPropertiesInterface;

// A status icon following the StatusNotifierItem specification.
// https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/StatusNotifierItem/
class StatusIconLinuxDbus : public views::StatusIconLinux {
 public:
  StatusIconLinuxDbus();
  ~StatusIconLinuxDbus() override;

  // StatusIcon:
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const base::string16& tool_tip) override;
  void UpdatePlatformContextMenu(ui::MenuModel* model) override;

 private:
  // Step 1: verify with the StatusNotifierWatcher that a StatusNotifierHost is
  // registered.
  void OnHostRegisteredResponse(dbus::Response* response);

  // Step 2: register a StatusNotifierItem service.
  void OnOwnership(const std::string& service_name, bool success);

  // Step 3: export methods for the StatusNotifierItem and the properties
  // interface.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);
  void OnInitialized(bool success);

  // Step 4: register the StatusNotifierItem with the StatusNotifierWatcher.
  void OnRegistered(dbus::Response* response);

  // DBus methods.
  // Action       -> KDE behavior:
  // Left-click   -> Activate
  // Right-click  -> ContextMenu
  // Scroll       -> Scroll
  // Middle-click -> SecondaryActivate
  void OnActivate(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender sender);
  void OnContextMenu(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender sender);
  void OnScroll(dbus::MethodCall* method_call,
                dbus::ExportedObject::ResponseSender sender);
  void OnSecondaryActivate(dbus::MethodCall* method_call,
                           dbus::ExportedObject::ResponseSender sender);

  scoped_refptr<dbus::Bus> bus_;

  int service_id_ = 0;
  dbus::ObjectProxy* watcher_ = nullptr;
  dbus::ExportedObject* item_ = nullptr;

  base::RepeatingCallback<void(bool)> barrier_;

  std::unique_ptr<DbusPropertiesInterface> properties_;

  base::WeakPtrFactory<StatusIconLinuxDbus> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconLinuxDbus);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_DBUS_H_
