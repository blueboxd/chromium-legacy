// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_client.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/url_constants.h"

namespace metrics {

MetricsServiceClient::MetricsServiceClient() {}

MetricsServiceClient::~MetricsServiceClient() {}

ukm::UkmService* MetricsServiceClient::GetUkmService() {
  return nullptr;
}

bool MetricsServiceClient::IsReportingPolicyManaged() {
  return false;
}

EnableMetricsDefault MetricsServiceClient::GetMetricsReportingDefaultState() {
  return EnableMetricsDefault::DEFAULT_UNKNOWN;
}

bool MetricsServiceClient::IsUMACellularUploadLogicEnabled() {
  return false;
}

GURL MetricsServiceClient::GetMetricsServerUrl() {
  return GURL(kNewMetricsServerUrl);
}

GURL MetricsServiceClient::GetInsecureMetricsServerUrl() {
  return GURL(kNewMetricsServerUrlInsecure);
}

bool MetricsServiceClient::SyncStateAllowsUkm() {
  return false;
}

bool MetricsServiceClient::SyncStateAllowsExtensionUkm() {
  return false;
}

bool MetricsServiceClient::AreNotificationListenersEnabledOnAllProfiles() {
  return false;
}

std::string MetricsServiceClient::GetAppPackageName() {
  return std::string();
}

std::string MetricsServiceClient::GetUploadSigningKey() {
  return std::string();
}

void MetricsServiceClient::SetUpdateRunningServicesCallback(
    const base::Closure& callback) {
  update_running_services_ = callback;
}

void MetricsServiceClient::UpdateRunningServices() {
  if (update_running_services_)
    update_running_services_.Run();
}

bool MetricsServiceClient::IsMetricsReportingForceEnabled() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceEnableMetricsReporting);
}

}  // namespace metrics
