// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/network_type_conversions.h"

#include "base/strings/string_number_conversions.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace sync_wifi {

std::string DecodeHexString(const std::string& base_16) {
  std::string decoded;
  DCHECK_EQ(base_16.size() % 2, 0u) << "Must be a multiple of 2";
  decoded.reserve(base_16.size() / 2);

  std::vector<uint8_t> v;
  if (!base::HexStringToBytes(base_16, &v)) {
    NOTREACHED();
  }
  decoded.assign(reinterpret_cast<const char*>(&v[0]), v.size());
  return decoded;
}

std::string SecurityTypeStringFromMojo(
    const network_config::mojom::SecurityType& security_type) {
  switch (security_type) {
    case network_config::mojom::SecurityType::kWpaPsk:
      return shill::kSecurityPsk;
    case network_config::mojom::SecurityType::kWepPsk:
      return shill::kSecurityWep;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      NOTREACHED();
      return "";
  }
}

std::string SecurityTypeStringFromProto(
    const sync_pb::WifiConfigurationSpecifics_SecurityType& security_type) {
  switch (security_type) {
    case sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_PSK:
      return shill::kSecurityPsk;
    case sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_WEP:
      return shill::kSecurityWep;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      NOTREACHED();
      return "";
  }
}

sync_pb::WifiConfigurationSpecifics_SecurityType SecurityTypeProtoFromMojo(
    const network_config::mojom::SecurityType& security_type) {
  switch (security_type) {
    case network_config::mojom::SecurityType::kWpaPsk:
      return sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_PSK;
    case network_config::mojom::SecurityType::kWepPsk:
      return sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_WEP;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      NOTREACHED();
      return sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_NONE;
  }
}

sync_pb::WifiConfigurationSpecifics_AutomaticallyConnectOption
AutomaticallyConnectProtoFromMojo(
    const network_config::mojom::ManagedBooleanPtr& auto_connect) {
  if (!auto_connect) {
    return sync_pb::WifiConfigurationSpecifics::
        AUTOMATICALLY_CONNECT_UNSPECIFIED;
  }

  if (auto_connect->active_value) {
    return sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_ENABLED;
  }

  return sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_DISABLED;
}

sync_pb::WifiConfigurationSpecifics_IsPreferredOption IsPreferredProtoFromMojo(
    const network_config::mojom::ManagedInt32Ptr& is_preferred) {
  if (!is_preferred) {
    return sync_pb::WifiConfigurationSpecifics::IS_PREFERRED_UNSPECIFIED;
  }

  if (is_preferred->active_value == 1) {
    return sync_pb::WifiConfigurationSpecifics::IS_PREFERRED_ENABLED;
  }

  return sync_pb::WifiConfigurationSpecifics::IS_PREFERRED_DISABLED;
}

sync_pb::WifiConfigurationSpecifics_ProxyConfiguration_ProxyOption
ProxyOptionProtoFromMojo(
    const network_config::mojom::ManagedProxySettingsPtr& proxy_settings) {
  if (!proxy_settings) {
    return sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_UNSPECIFIED;
  }

  if (proxy_settings->type->active_value == ::onc::proxy::kPAC) {
    return sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_AUTOMATIC;
  }

  if (proxy_settings->type->active_value == ::onc::proxy::kWPAD) {
    return sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_AUTODISCOVERY;
  }

  if (proxy_settings->type->active_value == ::onc::proxy::kManual) {
    return sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
        PROXY_OPTION_MANUAL;
  }

  return sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
      PROXY_OPTION_DISABLED;
}

sync_pb::WifiConfigurationSpecifics_ProxyConfiguration
ProxyConfigurationProtoFromMojo(
    const network_config::mojom::ManagedProxySettingsPtr& proxy_settings) {
  sync_pb::WifiConfigurationSpecifics_ProxyConfiguration proto;
  proto.set_proxy_option(ProxyOptionProtoFromMojo(proxy_settings));

  if (proto.proxy_option() ==
      sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
          PROXY_OPTION_AUTOMATIC) {
    if (proxy_settings->pac) {
      proto.set_autoconfiguration_url(proxy_settings->pac->active_value);
    }
  } else if (proto.proxy_option() ==
             sync_pb::WifiConfigurationSpecifics_ProxyConfiguration::
                 PROXY_OPTION_MANUAL) {
    sync_pb::
        WifiConfigurationSpecifics_ProxyConfiguration_ManualProxyConfiguration*
            manual_settings = proto.mutable_manual_proxy_configuration();
    manual_settings->set_http_proxy_url(
        proxy_settings->manual->http_proxy->host->active_value);
    manual_settings->set_http_proxy_port(
        proxy_settings->manual->http_proxy->port->active_value);
    manual_settings->set_secure_http_proxy_url(
        proxy_settings->manual->secure_http_proxy->host->active_value);
    manual_settings->set_secure_http_proxy_port(
        proxy_settings->manual->secure_http_proxy->port->active_value);
    manual_settings->set_socks_host_url(
        proxy_settings->manual->socks->host->active_value);
    manual_settings->set_socks_host_port(
        proxy_settings->manual->socks->port->active_value);
    for (const std::string& domain :
         proxy_settings->exclude_domains->active_value) {
      manual_settings->add_whitelisted_domains(domain);
    }
  }

  return proto;
}

network_config::mojom::SecurityType MojoSecurityTypeFromProto(
    const sync_pb::WifiConfigurationSpecifics_SecurityType& security_type) {
  switch (security_type) {
    case sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_PSK:
      return network_config::mojom::SecurityType::kWpaPsk;
    case sync_pb::WifiConfigurationSpecifics::SECURITY_TYPE_WEP:
      return network_config::mojom::SecurityType::kWepPsk;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      NOTREACHED();
      return network_config::mojom::SecurityType::kNone;
  }
}

network_config::mojom::ConfigPropertiesPtr MojoNetworkConfigFromProto(
    const sync_pb::WifiConfigurationSpecifics& specifics) {
  auto config = network_config::mojom::ConfigProperties::New();
  auto wifi = network_config::mojom::WiFiConfigProperties::New();

  wifi->ssid = DecodeHexString(specifics.hex_ssid());
  wifi->security = MojoSecurityTypeFromProto(specifics.security_type());
  wifi->passphrase = specifics.passphrase();

  config->type_config =
      network_config::mojom::NetworkTypeConfigProperties::NewWifi(
          std::move(wifi));

  config->auto_connect = network_config::mojom::AutoConnectConfig::New(
      specifics.automatically_connect() ==
      sync_pb::WifiConfigurationSpecifics::AUTOMATICALLY_CONNECT_ENABLED);

  config->priority = network_config::mojom::PriorityConfig::New(
      specifics.is_preferred() ==
              sync_pb::WifiConfigurationSpecifics::IS_PREFERRED_ENABLED
          ? 1
          : 0);

  if (specifics.custom_dns().size()) {
    auto ip_config = network_config::mojom::IPConfigProperties::New();
    std::vector<std::string> custom_dns;
    for (const std::string& nameserver : specifics.custom_dns()) {
      custom_dns.push_back(nameserver);
    }
    ip_config->name_servers = std::move(custom_dns);
    config->static_ip_config = std::move(ip_config);
    config->name_servers_config_type = onc::network_config::kIPConfigTypeStatic;
  } else {
    config->name_servers_config_type = onc::network_config::kIPConfigTypeDHCP;
  }

  return config;
}

const NetworkState* NetworkStateFromNetworkIdentifier(
    const NetworkIdentifier& id) {
  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      NetworkTypePattern::WiFi(), /*configured_only=*/true,
      /*visibleOnly=*/false, /*limit=*/0, &networks);
  for (const NetworkState* network : networks) {
    if (NetworkIdentifier::FromNetworkState(network) == id) {
      return network;
    }
  }
  return nullptr;
}

}  // namespace sync_wifi

}  // namespace chromeos
