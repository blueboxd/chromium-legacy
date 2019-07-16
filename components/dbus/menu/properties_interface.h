// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_MENU_PROPERTIES_INTERFACE_H_
#define COMPONENTS_DBUS_MENU_PROPERTIES_INTERFACE_H_

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "components/dbus/menu/types.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"

// https://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-properties
class COMPONENT_EXPORT(DBUS) DbusPropertiesInterface {
 public:
  using InitializedCallback = base::OnceCallback<void(bool success)>;

  // Registers method handlers for the properties interface.  The handlers will
  // not be removed until the bus is shut down.
  DbusPropertiesInterface(dbus::ExportedObject* exported_object,
                          InitializedCallback callback);
  ~DbusPropertiesInterface();

  void RegisterInterface(const std::string& interface);

  template <typename T>
  void SetProperty(const std::string& interface,
                   const std::string& name,
                   T&& value,
                   bool emit_signal = true,
                   bool send_change = true) {
    auto it = properties_.find(interface);
    DCHECK(it != properties_.end());
    (it->second)[name] = MakeDbusVariant(std::move(value));
    if (emit_signal)
      EmitPropertiesChangedSignal(interface, name, send_change);
  }

 private:
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  void OnGetAllProperties(dbus::MethodCall* method_call,
                          dbus::ExportedObject::ResponseSender response_sender);
  void OnGetProperty(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender);
  void OnSetProperty(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender);

  void EmitPropertiesChangedSignal(const std::string& interface,
                                   const std::string& property_name,
                                   bool send_change);

  dbus::ExportedObject* exported_object_ = nullptr;

  base::RepeatingCallback<void(bool)> barrier_;

  // A map from interface name to a map of properties.  The properties map is
  // from property name to property value.
  std::map<std::string, std::map<std::string, DbusVariant>> properties_;

  base::WeakPtrFactory<DbusPropertiesInterface> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DbusPropertiesInterface);
};

#endif  // COMPONENTS_DBUS_MENU_PROPERTIES_INTERFACE_H_
