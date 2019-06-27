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

namespace component_updater {

namespace {
// CRX hash. The extension id is: mokoelfideihaddlikjdmdbecmlibfaj.
const uint8_t kOnDeviceHeadSuggestPublicKeySHA256[32] = {
    0xce, 0xae, 0x4b, 0x58, 0x34, 0x87, 0x03, 0x3b, 0x8a, 0x93, 0xc3,
    0x14, 0x2c, 0xb8, 0x15, 0x09, 0x6e, 0x41, 0x6b, 0xc0, 0xb4, 0x0d,
    0x96, 0x2b, 0xd6, 0x7e, 0xd9, 0x97, 0xc6, 0x2a, 0xb0, 0x9f};

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
  const std::string* locale = manifest.FindStringPath("locale");

  if (!name || !locale)
    return false;

  if (!base::StartsWith(*name, "OnDeviceHeadSuggest",
                        base::CompareCase::SENSITIVE) ||
      *locale != accept_locale_)
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
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<OnDeviceHeadSuggestInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
