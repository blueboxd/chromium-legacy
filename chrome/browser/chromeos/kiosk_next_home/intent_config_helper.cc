// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/intent_config_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_value_converter.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/public/browser/system_connector.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "url/url_constants.h"

#if defined(KIOSK_NEXT) && defined(GOOGLE_CHROME_BUILD)
#include "chrome/grit/kiosk_next_internal_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif

namespace chromeos {
namespace kiosk_next_home {

namespace {

struct CustomScheme {
  std::string scheme;
  std::string path;

  static void RegisterJSONConverter(
      base::JSONValueConverter<CustomScheme>* converter) {
    converter->RegisterStringField("scheme", &CustomScheme::scheme);
    converter->RegisterStringField("path", &CustomScheme::path);
  }
};

struct IntentConfig {
  std::vector<std::unique_ptr<std::string>> allowed_hosts;
  std::vector<std::unique_ptr<CustomScheme>> allowed_custom_schemes;

  static void RegisterJSONConverter(
      base::JSONValueConverter<IntentConfig>* converter) {
    converter->RegisterRepeatedString("allowed_hosts",
                                      &IntentConfig::allowed_hosts);
    converter->RegisterRepeatedMessage("allowed_custom_schemes",
                                       &IntentConfig::allowed_custom_schemes);
  }
};

class ReadJsonConfigResourceDelegate : public IntentConfigHelper::Delegate {
 public:
  ReadJsonConfigResourceDelegate() = default;
  ~ReadJsonConfigResourceDelegate() override = default;

  // IntentConfigHelper::Delegate:
  std::string GetJsonConfig() const override {
#if defined(KIOSK_NEXT) && defined(GOOGLE_CHROME_BUILD)
    return ui::ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_KIOSK_NEXT_INTENT_CONFIG_JSON)
        .as_string();
#else
    return std::string();
#endif
  }

  DISALLOW_COPY_AND_ASSIGN(ReadJsonConfigResourceDelegate);
};

}  // namespace

// static
std::unique_ptr<IntentConfigHelper> IntentConfigHelper::GetInstance() {
  return std::make_unique<IntentConfigHelper>(
      std::make_unique<ReadJsonConfigResourceDelegate>());
}

IntentConfigHelper::IntentConfigHelper(std::unique_ptr<Delegate> delegate)
    : ready_(false) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](std::unique_ptr<Delegate> read_config_delegate) {
            return read_config_delegate->GetJsonConfig();
          },
          std::move(delegate)),
      base::BindOnce(&IntentConfigHelper::ParseConfig,
                     weak_factory_.GetWeakPtr()));
}

IntentConfigHelper::IntentConfigHelper() = default;

IntentConfigHelper::~IntentConfigHelper() = default;

bool IntentConfigHelper::IsIntentAllowed(const GURL& intent_uri) const {
  if (!ready_ || !intent_uri.is_valid())
    return false;

  if (intent_uri.IsStandard()) {
    if (intent_uri.SchemeIs(url::kHttpsScheme))
      return base::Contains(allowed_hosts_, intent_uri.host());
    // Other standard schemes besides https are not allowed.
    return false;
  }

  auto allowed_custom_scheme =
      allowed_custom_schemes_.find(intent_uri.scheme());
  if (allowed_custom_scheme == allowed_custom_schemes_.end())
    return false;
  // Scheme is allowed, so intent will be allowed if path matches the config.
  std::string path = allowed_custom_scheme->second;
  return path == intent_uri.path();
}

void IntentConfigHelper::ParseConfig(const std::string& json_config) {
  auto* connector = content::GetSystemConnector();
  // Service manager may not be initialized in tests.
  if (!connector)
    return;

  // Parse JSON in utility process via Data Decoder Service.
  data_decoder::SafeJsonParser::Parse(
      connector, json_config,
      base::BindOnce(&IntentConfigHelper::ParseConfigDone,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&IntentConfigHelper::ParseConfigFailed,
                     weak_factory_.GetWeakPtr()));
}

void IntentConfigHelper::ParseConfigDone(base::Value config_value) {
  base::JSONValueConverter<IntentConfig> converter;
  IntentConfig parsed_config;
  if (converter.Convert(config_value, &parsed_config)) {
    for (const auto& allowed_host_ptr : parsed_config.allowed_hosts)
      allowed_hosts_.emplace(std::move(*allowed_host_ptr));

    for (const auto& custom_scheme_ptr : parsed_config.allowed_custom_schemes) {
      allowed_custom_schemes_[std::move(custom_scheme_ptr->scheme)] =
          std::move(custom_scheme_ptr->path);
    }

    ready_ = true;
  }
}

void IntentConfigHelper::ParseConfigFailed(const std::string& error) {
  DLOG(ERROR) << "Failed to parse intent JSON config: " << error;
}

}  // namespace kiosk_next_home
}  // namespace chromeos
