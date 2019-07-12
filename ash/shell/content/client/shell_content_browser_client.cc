// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/client/shell_content_browser_client.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "ash/shell/content/client/shell_browser_main_parts.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/network_config/public/cpp/manifest.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"  // nogncheck
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#include "content/public/browser/browser_context.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/content_utility_client.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ash {
namespace shell {

namespace {

const service_manager::Manifest& GetAshShellBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .RequireCapability(device::mojom::kServiceName, "device:fingerprint")
          .RequireCapability(
              chromeos::network_config::mojom::kServiceName,
              chromeos::network_config::mojom::kNetworkConfigCapability)
          .Build()};
  return *manifest;
}

}  // namespace

ShellContentBrowserClient::ShellContentBrowserClient() = default;

ShellContentBrowserClient::~ShellContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
ShellContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  return std::make_unique<ShellBrowserMainParts>(parameters);
}

void ShellContentBrowserClient::GetQuotaSettings(
    content::BrowserContext* context,
    content::StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  // This should not be called in ash content environment.
  CHECK(false);
}

base::Optional<service_manager::Manifest>
ShellContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  // This is necessary for outgoing interface requests.
  if (name == content::mojom::kBrowserServiceName)
    return GetAshShellBrowserOverlayManifest();

  return base::nullopt;
}

std::vector<service_manager::Manifest>
ShellContentBrowserClient::GetExtraServiceManifests() {
  return std::vector<service_manager::Manifest>({
      chromeos::network_config::GetManifest(),
  });
}

}  // namespace shell
}  // namespace ash
