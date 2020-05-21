// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting_service_settings.h"

#include "components/policy/core/browser/url_util.h"

namespace enterprise_connectors {

ReportingServiceSettings::ReportingServiceSettings(
    const base::Value& settings_value) {
  if (!settings_value.is_dict())
    return;

  // The service provider identifier should always be there.
  const std::string* service_provider =
      settings_value.FindStringKey(kKeyServiceProvider);
  if (service_provider)
    service_provider_ = *service_provider;
}

base::Optional<ReportingSettings>
ReportingServiceSettings::GetReportingSettings() const {
  if (!IsValid())
    return base::nullopt;

  ReportingSettings settings;

  // TODO(rogerta): once service provider configs are implemented set the
  // reporting URL field.

  return settings;
}

bool ReportingServiceSettings::IsValid() const {
  // The settings are valid only if provider was given.
  return !service_provider_.empty();
}

ReportingServiceSettings::ReportingServiceSettings(ReportingServiceSettings&&) =
    default;
ReportingServiceSettings::~ReportingServiceSettings() = default;

}  // namespace enterprise_connectors
