// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_STRINGS_PROVIDER_H_

#include <vector>

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_per_page_strings_provider.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

class InternetStringsProvider
    : public OsSettingsPerPageStringsProvider,
      public network_config::mojom::CrosNetworkConfigObserver {
 public:
  InternetStringsProvider(Profile* profile, Delegate* per_page_delegate);
  ~InternetStringsProvider() override;

 private:
  // OsSettingsPerPageStringsProvider:
  void AddUiStrings(content::WebUIDataSource* html_source) const override;

  // network_config::mojom::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks)
      override {}
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override {}
  void OnNetworkStateListChanged() override {}
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}
  void OnDeviceStateListChanged() override;

  void FetchDeviceList();
  void OnDeviceList(
      std::vector<network_config::mojom::DeviceStatePropertiesPtr> devices);

  mojo::Receiver<network_config::mojom::CrosNetworkConfigObserver> receiver_{
      this};
  mojo::Remote<network_config::mojom::CrosNetworkConfig> cros_network_config_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_STRINGS_PROVIDER_H_
