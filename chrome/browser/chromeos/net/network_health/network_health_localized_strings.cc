// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health/network_health_localized_strings.h"

#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace network_health {

namespace {

constexpr webui::LocalizedString kLocalizedStrings[] = {
    // Network Health Summary Strings
    {"NetworkHealthState", IDS_NETWORK_HEALTH_STATE},
    {"NetworkHealthStateUninitialized", IDS_NETWORK_HEALTH_STATE_UNINITIALIZED},
    {"NetworkHealthStateDisabled", IDS_NETWORK_HEALTH_STATE_DISABLED},
    {"NetworkHealthStateProhibited", IDS_NETWORK_HEALTH_STATE_PROHIBITED},
    {"NetworkHealthStateNotConnected", IDS_NETWORK_HEALTH_STATE_NOT_CONNECTED},
    {"NetworkHealthStateConnecting", IDS_NETWORK_HEALTH_STATE_CONNECTING},
    {"NetworkHealthStatePortal", IDS_NETWORK_HEALTH_STATE_PORTAL},
    {"NetworkHealthStateConnected", IDS_NETWORK_HEALTH_STATE_CONNECTED},
    {"NetworkHealthStateOnline", IDS_NETWORK_HEALTH_STATE_ONLINE},

    // Network Diagnostics Strings
    {"NetworkDiagnosticsLanConnectivity",
     IDS_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY},
    {"NetworkDiagnosticsSignalStrength",
     IDS_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH},
    {"NetworkDiagnosticsGatewayCanBePinged",
     IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED},
    {"NetworkDiagnosticsHasSecureWiFiConnection",
     IDS_NETWORK_DIAGNOSTICS_HAS_SECURE_WIFI_CONNECTION},
    {"NetworkDiagnosticsDnsResolverPresent",
     IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT},
    {"NetworkDiagnosticsDnsLatency", IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY},
    {"NetworkDiagnosticsDnsResolution", IDS_NETWORK_DIAGNOSTICS_DNS_RESOLUTION},
    {"NetworkDiagnosticsPassed", IDS_NETWORK_DIAGNOSTICS_PASSED},
    {"NetworkDiagnosticsFailed", IDS_NETWORK_DIAGNOSTICS_FAILED},
    {"NetworkDiagnosticsNotRun", IDS_NETWORK_DIAGNOSTICS_NOT_RUN},
    {"NetworkDiagnosticsRun", IDS_NETWORK_DIAGNOSTICS_RUN},
    {"NetworkDiagnosticsRunAll", IDS_NETWORK_DIAGNOSTICS_RUN_ALL},
    {"NetworkDiagnosticsSendFeedback", IDS_NETWORK_DIAGNOSTICS_SEND_FEEDBACK},
};

}  // namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

}  // namespace network_health
}  // namespace chromeos
