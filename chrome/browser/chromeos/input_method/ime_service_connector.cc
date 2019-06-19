// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ime_service_connector.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"

namespace chromeos {
namespace input_method {

namespace {

bool IsImePathInvalid(const base::FilePath& file_path) {
  // Only non-empty, relative path which doesn't reference a parent is allowed.
  return file_path.empty() || file_path.IsAbsolute() ||
         file_path.ReferencesParent();
}

bool IsURLInvalid(const GURL& url) {
  // TODO(https://crbug.com/837156): Use URL whitelist instead of the general
  // checks below.
  return !url.DomainIs("dl.google.com") || !url.SchemeIs(url::kHttpsScheme);
}

}  // namespace

ImeServiceConnector::ImeServiceConnector(Profile* profile)
    : profile_(profile),
      instance_id_(base::Token::CreateRandom()),
      access_(this) {}

ImeServiceConnector::~ImeServiceConnector() = default;

void ImeServiceConnector::DownloadImeFileTo(
    const GURL& url,
    const base::FilePath& file_path,
    DownloadImeFileToCallback callback) {
  // TODO(https://crbug.com/837156): Download file by the network service.
  // Validate url and file_path, return an empty file path if not.
  if (IsURLInvalid(url) || IsImePathInvalid(file_path)) {
    base::FilePath empty_path;
    std::move(callback).Run(empty_path);
    return;
  }

  // Final path always starts from profile path.
  base::FilePath full_path = profile_->GetPath().Append(file_path);
  std::move(callback).Run(full_path);
}

void ImeServiceConnector::SetupImeService(
    mojo::PendingReceiver<chromeos::ime::mojom::InputEngineManager> receiver) {
  auto* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  auto per_id_filter = service_manager::ServiceFilter::ByNameWithId(
      chromeos::ime::mojom::kServiceName, instance_id_);

  // Connect to the ChromeOS IME service.
  if (!access_client_.is_bound()) {
    // Connect service as a PlatformAccessClient interface.
    connector->Connect(per_id_filter,
                       access_client_.BindNewPipeAndPassReceiver());

    access_client_->SetPlatformAccessProvider(
        access_.BindNewPipeAndPassRemote());
  }

  // Connect to the same service as a InputEngineManager interface.
  connector->Connect(per_id_filter, std::move(receiver));
}

void ImeServiceConnector::OnPlatformAccessConnectionLost() {
  // Reset the access_client_
  access_client_.reset();
}

}  // namespace input_method
}  // namespace chromeos
