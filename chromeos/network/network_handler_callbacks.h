// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_
#define CHROMEOS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace network_handler {

COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kDBusFailedError[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kDBusFailedErrorMessage[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kErrorName[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kErrorDetail[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kDbusErrorName[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kDbusErrorMessage[];

// An error callback used by both the configuration handler and the state
// handler to receive error results from the API.
typedef base::RepeatingCallback<void(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data)>
    ErrorCallback;

typedef base::OnceCallback<void(const std::string& service_path,
                                const base::DictionaryValue& dictionary)>
    DictionaryResultCallback;

typedef base::Callback<void(const std::string& string_result)>
    StringResultCallback;

typedef base::Callback<void(const std::string& service_path,
                            const std::string& guid)>
    ServiceResultCallback;

// Create a DictionaryValue for passing to ErrorCallback.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::DictionaryValue* CreateErrorData(const std::string& path,
                                       const std::string& error_name,
                                       const std::string& error_detail);

// If not NULL, runs |error_callback| with an ErrorData dictionary created from
// the other arguments.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void RunErrorCallback(const ErrorCallback& error_callback,
                      const std::string& path,
                      const std::string& error_name,
                      const std::string& error_detail);

COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::DictionaryValue* CreateDBusErrorData(
    const std::string& path,
    const std::string& error_name,
    const std::string& error_detail,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message);

// Callback for Shill errors.
// |error_name| is the error name passed to |error_callback|.
// |path| is the associated object path or blank if not relevant.
// |dbus_error_name| and |dbus_error_message| are provided by the DBus handler.
// Logs an error and calls |error_callback| if not null.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void ShillErrorCallbackFunction(const std::string& error_name,
                                const std::string& path,
                                const ErrorCallback& error_callback,
                                const std::string& dbus_error_name,
                                const std::string& dbus_error_message);

// Callback for property getters used by NetworkConfigurationHandler
// (for Network Services) and by NetworkDeviceHandler. Used to translate
// the DBus Dictionary callback into one that calls the error callback
// if |call_status| != DBUS_METHOD_CALL_SUCCESS.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void GetPropertiesCallback(DictionaryResultCallback callback,
                           const ErrorCallback& error_callback,
                           const std::string& path,
                           DBusMethodCallStatus call_status,
                           const base::DictionaryValue& value);

}  // namespace network_handler
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_
