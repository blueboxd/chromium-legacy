// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_SECTION_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

class SearchTagRegistry;

class InternetSection
    : public OsSettingsSection,
      public network_config::mojom::CrosNetworkConfigObserver {
 public:
  InternetSection(Profile* profile, SearchTagRegistry* search_tag_registry);
  ~InternetSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;
  std::string ModifySearchResultUrl(
      const SearchConcept& concept) const override;

  // network_config::mojom::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks)
      override;
  void OnDeviceStateListChanged() override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override {}
  void OnNetworkStateListChanged() override {}
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}

  void FetchDeviceList();
  void OnDeviceList(
      std::vector<network_config::mojom::DeviceStatePropertiesPtr> devices);

  void FetchActiveNetworks();
  void OnActiveNetworks(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks);

  // Note: If not connected, the below fields are null.
  base::Optional<std::string> connected_ethernet_guid_;
  base::Optional<std::string> connected_wifi_guid_;
  base::Optional<std::string> connected_cellular_guid_;
  base::Optional<std::string> connected_tether_guid_;
  base::Optional<std::string> connected_vpn_guid_;

  mojo::Receiver<network_config::mojom::CrosNetworkConfigObserver> receiver_{
      this};
  mojo::Remote<network_config::mojom::CrosNetworkConfig> cros_network_config_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_SECTION_H_
