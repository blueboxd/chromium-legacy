// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting_service_settings.h"

#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/policy/core/browser/url_util.h"

namespace enterprise_connectors {

ReportingServiceSettings::ReportingServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config) {
  if (!settings_value.is_dict())
    return;

  // The service provider identifier should always be there, and it should match
  // an existing provider.
  const std::string* service_provider_name =
      settings_value.FindStringKey(kKeyServiceProvider);
  if (service_provider_name) {
    service_provider_ =
        service_provider_config.GetServiceProvider(*service_provider_name);
  }
}

base::Optional<ReportingSettings>
ReportingServiceSettings::GetReportingSettings() const {
  if (!IsValid())
    return base::nullopt;

  ReportingSettings settings;

  settings.reporting_url = GURL(service_provider_->reporting_url());
  DCHECK(settings.reporting_url.is_valid());

  return settings;
}

bool ReportingServiceSettings::IsValid() const {
  // The settings are valid only if a provider was given.
  return service_provider_;
}

ReportingServiceSettings::ReportingServiceSettings(ReportingServiceSettings&&) =
    default;
ReportingServiceSettings::~ReportingServiceSettings() = default;

}  // namespace enterprise_connectors
