// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/on_device_head_suggest_component_installer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/omnibox/browser/on_device_model_update_listener.h"
#include "components/omnibox/common/omnibox_features.h"

namespace component_updater {

namespace {
// CRX hash. The extension id is: obedbbhbpmojnkanicioggnmelmoomoc.
const uint8_t kOnDeviceHeadSuggestPublicKeySHA256[32] = {
    0xe1, 0x43, 0x11, 0x71, 0xfc, 0xe9, 0xda, 0x0d, 0x82, 0x8e, 0x66,
    0xdc, 0x4b, 0xce, 0xec, 0xe2, 0xa3, 0xb0, 0x47, 0x00, 0x3d, 0xb8,
    0xcf, 0x8e, 0x0f, 0x4a, 0x73, 0xa1, 0x89, 0x1f, 0x5f, 0x38};

// Normalizes and returns current application locale, i.e capitalizes all
// letters and removes all hyphens and underscores in the locale string,
// e.g. "en-US" -> "ENUS".
std::string GetNormalizedLocale() {
  std::string locale = g_browser_process->GetApplicationLocale();
  for (const auto c : "-_")
    locale.erase(std::remove(locale.begin(), locale.end(), c), locale.end());

  std::transform(locale.begin(), locale.end(), locale.begin(),
                 [](char c) -> char { return base::ToUpperASCII(c); });
  return locale;
}

}  // namespace

OnDeviceHeadSuggestInstallerPolicy::OnDeviceHeadSuggestInstallerPolicy()
    : accept_locale_(GetNormalizedLocale()) {}

OnDeviceHeadSuggestInstallerPolicy::~OnDeviceHeadSuggestInstallerPolicy() =
    default;

bool OnDeviceHeadSuggestInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  const std::string* name = manifest.FindStringPath("name");

  if (!name || *name != ("OnDeviceHeadSuggest" + accept_locale_))
    return false;

  return base::PathExists(install_dir);
}

bool OnDeviceHeadSuggestInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool OnDeviceHeadSuggestInstallerPolicy::RequiresNetworkEncryption() const {
  return true;
}

update_client::CrxInstaller::Result
OnDeviceHeadSuggestInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void OnDeviceHeadSuggestInstallerPolicy::OnCustomUninstall() {}

void OnDeviceHeadSuggestInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  auto* listener = OnDeviceModelUpdateListener::GetInstance();
  if (listener)
    listener->OnModelUpdate(install_dir);
}

base::FilePath OnDeviceHeadSuggestInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("OnDeviceHeadSuggestModel"));
}

void OnDeviceHeadSuggestInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kOnDeviceHeadSuggestPublicKeySHA256),
               std::end(kOnDeviceHeadSuggestPublicKeySHA256));
}

std::string OnDeviceHeadSuggestInstallerPolicy::GetName() const {
  return "OnDeviceHeadSuggest";
}

update_client::InstallerAttributes
OnDeviceHeadSuggestInstallerPolicy::GetInstallerAttributes() const {
  return {{"accept_locale", accept_locale_}};
}

std::vector<std::string> OnDeviceHeadSuggestInstallerPolicy::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

void RegisterOnDeviceHeadSuggestComponent(ComponentUpdateService* cus) {
  if (base::FeatureList::IsEnabled(omnibox::kOnDeviceHeadProvider)) {
    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<OnDeviceHeadSuggestInstallerPolicy>());
    installer->Register(cus, base::OnceClosure());
  }
}

}  // namespace component_updater
