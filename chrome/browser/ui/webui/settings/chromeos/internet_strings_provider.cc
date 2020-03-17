// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/internet_strings_provider.h"

#include "ash/public/cpp/network_config_service.h"
#include "base/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetNetworkSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_SETTINGS_TAG_NETWORK_SETTINGS,
       chrome::kNetworksSubPage,
       mojom::SearchResultIcon::kWifi,
       {IDS_SETTINGS_TAG_NETWORK_SETTINGS_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetEthernetSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_SETTINGS_TAG_ETHERNET_SETTINGS,
       chrome::kEthernetSettingsSubPage,
       mojom::SearchResultIcon::kEthernet,
       {IDS_SETTINGS_TAG_ETHERNET_SETTINGS_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_SETTINGS_TAG_WIFI_SETTINGS, chrome::kWiFiSettingsSubPage,
       mojom::SearchResultIcon::kWifi},
      {IDS_SETTINGS_TAG_TURN_ON_WIFI,
       chrome::kWiFiSettingsSubPage,
       mojom::SearchResultIcon::kWifi,
       {IDS_SETTINGS_TAG_TURN_ON_WIFI_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_SETTINGS_TAG_TURN_OFF_WIFI,
       chrome::kWiFiSettingsSubPage,
       mojom::SearchResultIcon::kWifi,
       {IDS_SETTINGS_TAG_TURN_OFF_WIFI_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_SETTINGS_TAG_CONNECT_WIFI, chrome::kWiFiSettingsSubPage,
       mojom::SearchResultIcon::kWifi},
      {IDS_SETTINGS_TAG_DISCONNECT_WIFI, chrome::kWiFiSettingsSubPage,
       mojom::SearchResultIcon::kWifi},
  });
  return *tags;
}

}  // namespace

InternetStringsProvider::InternetStringsProvider(Profile* profile,
                                                 Delegate* per_page_delegate)
    : OsSettingsPerPageStringsProvider(profile, per_page_delegate) {
  // General network search tags are always added.
  delegate()->AddSearchTags(GetNetworkSearchConcepts());

  // Receive updates when devices (e.g., Ethernet, Wi-Fi) go on/offline.
  ash::GetNetworkConfigService(
      cros_network_config_.BindNewPipeAndPassReceiver());
  cros_network_config_->AddObserver(receiver_.BindNewPipeAndPassRemote());

  // Fetch initial list of devices.
  FetchDeviceList();
}

InternetStringsProvider::~InternetStringsProvider() = default;

void InternetStringsProvider::AddUiStrings(
    content::WebUIDataSource* html_source) const {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"internetAddConnection", IDS_SETTINGS_INTERNET_ADD_CONNECTION},
      {"internetAddConnectionExpandA11yLabel",
       IDS_SETTINGS_INTERNET_ADD_CONNECTION_EXPAND_ACCESSIBILITY_LABEL},
      {"internetAddConnectionNotAllowed",
       IDS_SETTINGS_INTERNET_ADD_CONNECTION_NOT_ALLOWED},
      {"internetAddThirdPartyVPN", IDS_SETTINGS_INTERNET_ADD_THIRD_PARTY_VPN},
      {"internetAddVPN", IDS_SETTINGS_INTERNET_ADD_VPN},
      {"internetAddWiFi", IDS_SETTINGS_INTERNET_ADD_WIFI},
      {"internetConfigName", IDS_SETTINGS_INTERNET_CONFIG_NAME},
      {"internetDetailPageTitle", IDS_SETTINGS_INTERNET_DETAIL},
      {"internetDeviceEnabling", IDS_SETTINGS_INTERNET_DEVICE_ENABLING},
      {"internetDeviceInitializing", IDS_SETTINGS_INTERNET_DEVICE_INITIALIZING},
      {"internetJoinType", IDS_SETTINGS_INTERNET_JOIN_TYPE},
      {"internetKnownNetworksPageTitle", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS},
      {"internetMobileSearching", IDS_SETTINGS_INTERNET_MOBILE_SEARCH},
      {"internetNoNetworks", IDS_SETTINGS_INTERNET_NO_NETWORKS},
      {"internetPageTitle", IDS_SETTINGS_INTERNET},
      {"internetSummaryButtonA11yLabel",
       IDS_SETTINGS_INTERNET_SUMMARY_BUTTON_ACCESSIBILITY_LABEL},
      {"internetToggleMobileA11yLabel",
       IDS_SETTINGS_INTERNET_TOGGLE_MOBILE_ACCESSIBILITY_LABEL},
      {"internetToggleTetherLabel", IDS_SETTINGS_INTERNET_TOGGLE_TETHER_LABEL},
      {"internetToggleTetherSubtext",
       IDS_SETTINGS_INTERNET_TOGGLE_TETHER_SUBTEXT},
      {"internetToggleWiFiA11yLabel",
       IDS_SETTINGS_INTERNET_TOGGLE_WIFI_ACCESSIBILITY_LABEL},
      {"knownNetworksAll", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_ALL},
      {"knownNetworksButton", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_BUTTON},
      {"knownNetworksMessage", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MESSAGE},
      {"knownNetworksPreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_PREFFERED},
      {"knownNetworksMenuAddPreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_ADD_PREFERRED},
      {"knownNetworksMenuRemovePreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_REMOVE_PREFERRED},
      {"knownNetworksMenuForget",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_FORGET},
      {"networkAllowDataRoaming",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING},
      {"networkAllowDataRoamingEnabledHome",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING_ENABLED_HOME},
      {"networkAllowDataRoamingEnabledRoaming",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING_ENABLED_ROAMING},
      {"networkAllowDataRoamingDisabled",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING_DISABLED},
      {"networkAlwaysOnVpn", IDS_SETTINGS_INTERNET_NETWORK_ALWAYS_ON_VPN},
      {"networkAutoConnect", IDS_SETTINGS_INTERNET_NETWORK_AUTO_CONNECT},
      {"networkAutoConnectCellular",
       IDS_SETTINGS_INTERNET_NETWORK_AUTO_CONNECT_CELLULAR},
      {"networkButtonActivate", IDS_SETTINGS_INTERNET_BUTTON_ACTIVATE},
      {"networkButtonConfigure", IDS_SETTINGS_INTERNET_BUTTON_CONFIGURE},
      {"networkButtonConnect", IDS_SETTINGS_INTERNET_BUTTON_CONNECT},
      {"networkButtonDisconnect", IDS_SETTINGS_INTERNET_BUTTON_DISCONNECT},
      {"networkButtonForget", IDS_SETTINGS_INTERNET_BUTTON_FORGET},
      {"networkButtonViewAccount", IDS_SETTINGS_INTERNET_BUTTON_VIEW_ACCOUNT},
      {"networkConnectNotAllowed", IDS_SETTINGS_INTERNET_CONNECT_NOT_ALLOWED},
      {"networkIPAddress", IDS_SETTINGS_INTERNET_NETWORK_IP_ADDRESS},
      {"networkIPConfigAuto", IDS_SETTINGS_INTERNET_NETWORK_IP_CONFIG_AUTO},
      {"networkNameserversLearnMore", IDS_LEARN_MORE},
      {"networkPrefer", IDS_SETTINGS_INTERNET_NETWORK_PREFER},
      {"networkPrimaryUserControlled",
       IDS_SETTINGS_INTERNET_NETWORK_PRIMARY_USER_CONTROLLED},
      {"networkScanningLabel", IDS_NETWORK_SCANNING_MESSAGE},
      {"networkSectionAdvanced",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_ADVANCED},
      {"networkSectionAdvancedA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_ADVANCED_ACCESSIBILITY_LABEL},
      {"networkSectionNetwork", IDS_SETTINGS_INTERNET_NETWORK_SECTION_NETWORK},
      {"networkSectionNetworkExpandA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_NETWORK_ACCESSIBILITY_LABEL},
      {"networkSectionProxy", IDS_SETTINGS_INTERNET_NETWORK_SECTION_PROXY},
      {"networkSectionProxyExpandA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_PROXY_ACCESSIBILITY_LABEL},
      {"networkShared", IDS_SETTINGS_INTERNET_NETWORK_SHARED},
      {"networkVpnBuiltin", IDS_NETWORK_TYPE_VPN_BUILTIN},
      {"networkOutOfRange", IDS_SETTINGS_INTERNET_WIFI_NETWORK_OUT_OF_RANGE},
      {"cellularContactSpecificCarrier",
       IDS_SETTINGS_INTERNET_CELLULAR_CONTACT_SPECIFIC_CARRIER},
      {"cellularContactDefaultCarrier",
       IDS_SETTINGS_INTERNET_CELLULAR_CONTACT_DEFAULT_CARRIER},
      {"tetherPhoneOutOfRange",
       IDS_SETTINGS_INTERNET_TETHER_PHONE_OUT_OF_RANGE},
      {"gmscoreNotificationsTitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_TITLE},
      {"gmscoreNotificationsOneDeviceSubtitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_ONE_DEVICE_SUBTITLE},
      {"gmscoreNotificationsTwoDevicesSubtitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_TWO_DEVICES_SUBTITLE},
      {"gmscoreNotificationsManyDevicesSubtitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_MANY_DEVICES_SUBTITLE},
      {"gmscoreNotificationsFirstStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_FIRST_STEP},
      {"gmscoreNotificationsSecondStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_SECOND_STEP},
      {"gmscoreNotificationsThirdStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_THIRD_STEP},
      {"gmscoreNotificationsFourthStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_FOURTH_STEP},
      {"tetherConnectionDialogTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DIALOG_TITLE},
      {"tetherConnectionAvailableDeviceTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_AVAILABLE_DEVICE_TITLE},
      {"tetherConnectionBatteryPercentage",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_BATTERY_PERCENTAGE},
      {"tetherConnectionExplanation",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_EXPLANATION},
      {"tetherConnectionCarrierWarning",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_CARRIER_WARNING},
      {"tetherConnectionDescriptionTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_TITLE},
      {"tetherConnectionDescriptionMobileData",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_MOBILE_DATA},
      {"tetherConnectionDescriptionBattery",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_BATTERY},
      {"tetherConnectionDescriptionWiFi",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_WIFI},
      {"tetherConnectionNotNowButton",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_NOT_NOW_BUTTON},
      {"tetherConnectionConnectButton",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_CONNECT_BUTTON},
      {"tetherEnableBluetooth", IDS_ENABLE_BLUETOOTH},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  chromeos::network_element::AddLocalizedStrings(html_source);
  chromeos::network_element::AddOncLocalizedStrings(html_source);
  chromeos::network_element::AddDetailsLocalizedStrings(html_source);
  chromeos::network_element::AddConfigLocalizedStrings(html_source);
  chromeos::network_element::AddErrorLocalizedStrings(html_source);

  html_source->AddString("networkGoogleNameserversLearnMoreUrl",
                         chrome::kGoogleNameserversLearnMoreURL);
  html_source->AddString(
      "internetNoNetworksMobileData",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_LOOKING_FOR_MOBILE_NETWORK,
          GetHelpUrlWithBoard(chrome::kInstantTetheringLearnMoreURL)));
}

void InternetStringsProvider::OnDeviceStateListChanged() {
  FetchDeviceList();
}

void InternetStringsProvider::FetchDeviceList() {
  cros_network_config_->GetDeviceStateList(base::BindOnce(
      &InternetStringsProvider::OnDeviceList, base::Unretained(this)));
}

void InternetStringsProvider::OnDeviceList(
    std::vector<network_config::mojom::DeviceStatePropertiesPtr> devices) {
  // Start with no search tags.
  delegate()->RemoveSearchTags(GetEthernetSearchConcepts());
  delegate()->RemoveSearchTags(GetWifiSearchConcepts());

  // Add a search tag each time we see a device type.
  for (const auto& device : devices) {
    if (device->type == network_config::mojom::NetworkType::kEthernet)
      delegate()->AddSearchTags(GetEthernetSearchConcepts());
    else if (device->type == network_config::mojom::NetworkType::kWiFi)
      delegate()->AddSearchTags(GetWifiSearchConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
