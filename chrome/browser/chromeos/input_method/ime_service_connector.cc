// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ime_service_connector.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#include "content/public/browser/system_connector.h"

#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace chromeos {
namespace input_method {

namespace {

constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("ime_url_downloader", R"(
    semantics {
      sender: "IME Service Downloader"
      description:
        "When user selects a new input method in ChromeOS, it may request a"
        "corresponding language module downloaded if it does not exist."
      trigger: "User switches to an input method without language module."
      data:
        "The language module download URL. No user identifier is sent."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
      policy_exception_justification:
        "Not implemented, considered not useful."
    })");

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
      url_loader_factory_(profile->GetURLLoaderFactory()),
      instance_id_(base::Token::CreateRandom()),
      access_(this) {}

ImeServiceConnector::~ImeServiceConnector() = default;

void ImeServiceConnector::DownloadImeFileTo(
    const GURL& url,
    const base::FilePath& file_path,
    DownloadImeFileToCallback callback) {
  // For now, we don't allow the client to download multi files at same time.
  // Downloading request will be aborted and return empty before the current
  // downloading task exits.
  // TODO(https://crbug.com/971954): Support multi downloads.
  // Validate url and file_path, return an empty file path if not.
  if (url_loader_ || IsURLInvalid(url) || IsImePathInvalid(file_path)) {
    base::FilePath empty_path;
    std::move(callback).Run(empty_path);
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  // Disable cookies for this request.
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES |
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  // Download the language module into a preconfigured ime folder of current
  // user's home which is allowed in IME service's sandbox.
  base::FilePath full_path = profile_->GetPath().Append(file_path);
  url_loader_->DownloadToFile(
      url_loader_factory_.get(),
      base::BindOnce(&ImeServiceConnector::OnFileDownloadComplete,
                     base::Unretained(this), std::move(callback)),
      full_path);
}

void ImeServiceConnector::SetupImeService(
    mojo::PendingReceiver<chromeos::ime::mojom::InputEngineManager> receiver) {
  auto* connector = content::GetSystemConnector();
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

void ImeServiceConnector::OnFileDownloadComplete(
    DownloadImeFileToCallback client_callback,
    base::FilePath path) {
  std::move(client_callback).Run(path);
  url_loader_.reset();
  return;
}

}  // namespace input_method
}  // namespace chromeos
