// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_launch_from_native.h"

#include <memory>
#include <utility>

#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/natively_connectable_handler.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace {

ScopedAllowNativeAppConnectionForTest* g_allow_native_app_connection_for_test =
    nullptr;

}  // namespace

bool ExtensionSupportsConnectionFromNativeApp(const std::string& extension_id,
                                              const std::string& host_id,
                                              Profile* profile,
                                              bool log_errors) {
  if (g_allow_native_app_connection_for_test) {
    return g_allow_native_app_connection_for_test->allow();
  }
  if (profile->IsOffTheRecord()) {
    return false;
  }
  auto* extension =
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          extension_id);
  if (!extension) {
    LOG_IF(ERROR, log_errors)
        << "Failed to launch native messaging connection: Unknown extension ID "
        << extension_id;
    return false;
  }
  const auto* natively_connectable_hosts =
      NativelyConnectableHosts::GetConnectableNativeMessageHosts(*extension);
  if (!natively_connectable_hosts ||
      !natively_connectable_hosts->count(host_id)) {
    LOG_IF(ERROR, log_errors)
        << "Extension \"" << extension_id << "\" does not list \"" << host_id
        << "\" in its natively_connectable manifest field";
    return false;
  }
  if (!extension->permissions_data()->active_permissions().HasAPIPermission(
          "nativeMessaging")) {
    LOG_IF(ERROR, log_errors)
        << "Extension \"" << extension_id
        << "\" does not have the \"nativeMessaging\" permission";
    return false;
  }
  if (!extension->permissions_data()->active_permissions().HasAPIPermission(
          "transientBackground")) {
    LOG_IF(ERROR, log_errors)
        << "Extension \"" << extension_id
        << "\" does not have the \"transientBackground\" permission";
    return false;
  }
  if (!EventRouter::Get(profile)->ExtensionHasEventListener(
          extension_id, "runtime.onConnectNative")) {
    LOG_IF(ERROR, log_errors)
        << "Failed to launch native messaging connection: Extension \""
        << extension_id << "\" is not listening for runtime.onConnectNative";
    return false;
  }

  return true;
}

ScopedAllowNativeAppConnectionForTest::ScopedAllowNativeAppConnectionForTest(
    bool allow)
    : allow_(allow) {
  DCHECK(!g_allow_native_app_connection_for_test);
  g_allow_native_app_connection_for_test = this;
}

ScopedAllowNativeAppConnectionForTest::
    ~ScopedAllowNativeAppConnectionForTest() {
  DCHECK_EQ(this, g_allow_native_app_connection_for_test);
  g_allow_native_app_connection_for_test = nullptr;
}

void LaunchNativeMessageHostFromNativeApp(const std::string& extension_id,
                                          const std::string& host_id,
                                          Profile* profile) {
  if (!ExtensionSupportsConnectionFromNativeApp(extension_id, host_id, profile,
                                                /* log_errors = */ true)) {
    // TODO(crbug.com/967262): Report errors to the native messaging host.
    return;
  }
  const extensions::PortId port_id(base::UnguessableToken::Create(),
                                   1 /* port_number */, true /* is_opener */);
  extensions::MessageService* const message_service =
      extensions::MessageService::Get(profile);
  // TODO(crbug.com/967262): Apply policy for allow_user_level.
  auto native_message_host = NativeMessageProcessHost::CreateWithLauncher(
      extension_id, host_id,
      NativeProcessLauncher::CreateDefault(
          /* allow_user_level = */ true, /* native_view = */ nullptr,
          profile->GetPath(),
          /* require_native_initiated_connections = */ true));
  auto native_message_port = std::make_unique<extensions::NativeMessagePort>(
      message_service->GetChannelDelegate(), port_id,
      std::move(native_message_host));
  message_service->OpenChannelToExtension(
      extensions::ChannelEndpoint(profile), port_id,
      extensions::MessagingEndpoint::ForNativeApp(host_id),
      std::move(native_message_port), extension_id, GURL(),
      std::string() /* channel_name */);
}

}  // namespace extensions
