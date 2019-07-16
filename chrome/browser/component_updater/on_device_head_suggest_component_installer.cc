// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/on_device_head_suggest_component_installer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/omnibox/common/omnibox_features.h"

namespace component_updater {

namespace {
// CRX hash. The extension id is: feejiaigafnbpeeogmhmjfmkcjplcneb.
const uint8_t kOnDeviceHeadSuggestPublicKeySHA256[32] = {
    0x54, 0x49, 0x80, 0x86, 0x05, 0xd1, 0xf4, 0x4e, 0x6c, 0x7c, 0x95,
    0xca, 0x29, 0xfb, 0x2d, 0x41, 0xe5, 0x12, 0x55, 0x83, 0x59, 0x58,
    0x50, 0x41, 0x02, 0x6b, 0xb9, 0x06, 0x2c, 0xe0, 0xf8, 0x34};

void UpdateModelDirectory(const base::FilePath& file_path) {
  base::PathService::Override(DIR_ON_DEVICE_HEAD_SUGGEST, file_path);
}

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
  base::PostTaskWithTraits(FROM_HERE,
                           {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
                           base::BindOnce(&UpdateModelDirectory, install_dir));
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
