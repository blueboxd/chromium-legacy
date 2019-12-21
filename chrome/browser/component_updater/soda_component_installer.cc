// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/soda_component_installer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "crypto/sha2.h"

using content::BrowserThread;

namespace component_updater {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the archive.
// TODO(evliu): The SODA library isn't ready to be exposed to the public yet so
// we should not check in the SHA256.
const uint8_t kSODAPublicKeySHA256[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const base::FilePath::CharType kSODABinaryFileName[] =
    FILE_PATH_LITERAL("SODAFiles/libsoda.experimental.so");

static_assert(base::size(kSODAPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

const char kSODAManifestName[] = "SODA Library";

}  // namespace

SODAComponentInstallerPolicy::SODAComponentInstallerPolicy(
    const OnSODAComponentReadyCallback& callback)
    : on_component_ready_callback_(callback) {}

SODAComponentInstallerPolicy::~SODAComponentInstallerPolicy() = default;

const std::string SODAComponentInstallerPolicy::GetExtensionId() {
  return crx_file::id_util::GenerateIdFromHash(kSODAPublicKeySHA256,
                                               sizeof(kSODAPublicKeySHA256));
}

void SODAComponentInstallerPolicy::UpdateSODAComponentOnDemand() {
  const std::string crx_id =
      component_updater::SODAComponentInstallerPolicy::GetExtensionId();
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS)
          LOG(ERROR) << "On demand update of the SODA component failed "
                        "with error: "
                     << static_cast<int>(error);
      }));
}

bool SODAComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kSODABinaryFileName));
}

bool SODAComponentInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return true;
}

bool SODAComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
SODAComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void SODAComponentInstallerPolicy::OnCustomUninstall() {}

void SODAComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  on_component_ready_callback_.Run(install_dir);
}

base::FilePath SODAComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("SODA"));
}

void SODAComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kSODAPublicKeySHA256,
               kSODAPublicKeySHA256 + base::size(kSODAPublicKeySHA256));
}

std::string SODAComponentInstallerPolicy::GetName() const {
  return kSODAManifestName;
}

update_client::InstallerAttributes
SODAComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> SODAComponentInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterSODAComponent(ComponentUpdateService* cus,
                           PrefService* prefs,
                           base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(evliu): The SODA library isn't ready to be exposed to the public yet
  // we should not register the component.
  NOTIMPLEMENTED();
}
}  // namespace component_updater
