// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/service_providers.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"

namespace {

const base::FilePath::CharType kConfigFileName[] =
    FILE_PATH_LITERAL("service_providers.json");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: dfcoifdifjfolmglbbogapfcihdgckga
// SHA256: 352e8538595ebc6b11e60f5287362a60a4c4f0c2fb3595c43621942ffbd973b5
const uint8_t kPublicKeySHA256[32] = {
    0x35, 0x2e, 0x85, 0x38, 0x59, 0x5e, 0xbc, 0x6b, 0x11, 0xe6, 0x0f,
    0x52, 0x87, 0x36, 0x2a, 0x60, 0xa4, 0xc4, 0xf0, 0xc2, 0xfb, 0x35,
    0x95, 0xc4, 0x36, 0x21, 0x94, 0x2f, 0xfb, 0xd9, 0x73, 0xb5};

base::Optional<base::Value> LoadConfigFromDisk(const base::FilePath& path) {
  // TODO(crbug/1081375): replace this with a safe read function.
  std::string json;
  if (!base::ReadFileToString(path, &json)) {
    return base::nullopt;
  }

  return base::JSONReader::Read(json);
}

}  // namespace

namespace enterprise_connectors {

class ServiceProvidersConfigPolicy
    : public component_updater::ComponentInstallerPolicy {
 public:
  ServiceProvidersConfigPolicy();
  ~ServiceProvidersConfigPolicy() override;

  const base::Value& GetServiceProvidersConfig() const { return config_; }

 private:
  void SetConfig(base::Optional<base::Value> config);

  // component_updater::ComponentInstallerPolicy overrides:
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  std::vector<std::string> GetMimeTypes() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  base::Value config_;
  base::WeakPtrFactory<ServiceProvidersConfigPolicy> factory_{this};
};

ServiceProvidersConfigPolicy::ServiceProvidersConfigPolicy() = default;
ServiceProvidersConfigPolicy::~ServiceProvidersConfigPolicy() = default;

void ServiceProvidersConfigPolicy::SetConfig(
    base::Optional<base::Value> config) {
  if (!config.has_value())
    return;

  config_ = std::move(config.value());
}

bool ServiceProvidersConfigPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  const base::FilePath config_file = install_dir.Append(kConfigFileName);
  return base::PathExists(config_file);
}

bool ServiceProvidersConfigPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return false;
}

bool ServiceProvidersConfigPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
ServiceProvidersConfigPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void ServiceProvidersConfigPolicy::OnCustomUninstall() {}

void ServiceProvidersConfigPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  const base::FilePath config_file = install_dir.Append(kConfigFileName);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadConfigFromDisk, config_file),
      base::BindOnce(&ServiceProvidersConfigPolicy::SetConfig,
                     factory_.GetWeakPtr()));
}

base::FilePath ServiceProvidersConfigPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("ECSerivceProvidersConfig"));
}

void ServiceProvidersConfigPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kPublicKeySHA256,
               kPublicKeySHA256 + base::size(kPublicKeySHA256));
}

std::string ServiceProvidersConfigPolicy::GetName() const {
  return "Enterprise Connectors Service Providers Configuration";
}

std::vector<std::string> ServiceProvidersConfigPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

update_client::InstallerAttributes
ServiceProvidersConfigPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterServiceProvidersComponent(
    component_updater::ComponentUpdateService* cus) {
  auto installer = base::MakeRefCounted<component_updater::ComponentInstaller>(
      std::make_unique<ServiceProvidersConfigPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace enterprise_connectors
