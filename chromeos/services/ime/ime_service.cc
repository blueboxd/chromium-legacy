// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "build/buildflag.h"
#include "chromeos/services/ime/public/cpp/buildflags.h"

#if BUILDFLAG(ENABLE_CROS_IME_DECODER)
#include "chromeos/services/ime/decoder/decoder_engine.h"
#endif

namespace chromeos {
namespace ime {

namespace {

enum SimpleDownloadError {
  SIMPLE_DOWNLOAD_ERROR_OK = 0,
  SIMPLE_DOWNLOAD_ERROR_FAILED = -1,
  SIMPLE_DOWNLOAD_ERROR_ABORTED = -2,
};

}  // namespace

ImeService::ImeService(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver)
    : service_binding_(this, std::move(receiver)) {}

ImeService::~ImeService() = default;

void ImeService::OnStart() {
  binders_.Add(base::BindRepeating(&ImeService::AddInputEngineManagerReceiver,
                                   base::Unretained(this)));

  binders_.Add(base::BindRepeating(
      &ImeService::BindPlatformAccessClientReceiver, base::Unretained(this)));

  manager_receivers_.set_disconnect_handler(base::BindRepeating(
      &ImeService::OnConnectionLost, base::Unretained(this)));

#if BUILDFLAG(ENABLE_CROS_IME_DECODER)
  input_engine_ = std::make_unique<DecoderEngine>(this);
#else
  input_engine_ = std::make_unique<InputEngine>();
#endif
}

void ImeService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  binders_.TryBind(interface_name, &receiver_pipe);
}

void ImeService::ConnectToImeEngine(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputChannel> to_engine_request,
    mojo::PendingRemote<mojom::InputChannel> from_engine,
    const std::vector<uint8_t>& extra,
    ConnectToImeEngineCallback callback) {
  DCHECK(input_engine_);
  bool bound = input_engine_->BindRequest(
      ime_spec, std::move(to_engine_request), std::move(from_engine), extra);
  std::move(callback).Run(bound);
}

void ImeService::AddInputEngineManagerReceiver(
    mojo::PendingReceiver<mojom::InputEngineManager> receiver) {
  manager_receivers_.Add(this, std::move(receiver));
  // TODO(https://crbug.com/837156): Reset the cleanup timer.
}

void ImeService::BindPlatformAccessClientReceiver(
    mojo::PendingReceiver<mojom::PlatformAccessClient> receiver) {
  if (!access_receiver_.is_bound()) {
    access_receiver_.Bind(std::move(receiver));
  }
}

void ImeService::SetPlatformAccessProvider(
    mojo::PendingRemote<mojom::PlatformAccessProvider> access) {
  platform_access_.Bind(std::move(access));
}

void ImeService::OnConnectionLost() {
  if (manager_receivers_.empty()) {
    service_binding_.RequestClose();
    // TODO(https://crbug.com/837156): Set a timer to start a cleanup.
  }
}

void ImeService::SimpleDownloadFinished(SimpleDownloadCallback callback,
                                        const base::FilePath& file) {
  if (file.empty()) {
    callback(SIMPLE_DOWNLOAD_ERROR_FAILED, "");
  } else {
    callback(SIMPLE_DOWNLOAD_ERROR_OK, file.MaybeAsASCII().c_str());
  }
}

const char* ImeService::GetImeBundleDir() {
  return "";
}

const char* ImeService::GetImeGlobalDir() {
  // Global IME data dir will not be supported yet.
  return "";
}

const char* ImeService::GetImeUserHomeDir() {
  return "";
}

int ImeService::SimpleDownloadToFile(const char* url,
                                     const char* file_path,
                                     SimpleDownloadCallback callback) {
  if (!platform_access_.is_bound()) {
    callback(SIMPLE_DOWNLOAD_ERROR_ABORTED, "");
  } else {
    GURL download_url(url);
    // |file_path| must be relative.
    base::FilePath relative_file_path(file_path);

    platform_access_->DownloadImeFileTo(
        download_url, relative_file_path,
        base::BindOnce(&ImeService::SimpleDownloadFinished,
                       base::Unretained(this), std::move(callback)));
  }

  // For |SimpleDownloadToFile|, always returns 0.
  return 0;
}

ImeCrosDownloader* ImeService::GetDownloader() {
  // TODO(https://crbug.com/837156): Create an ImeCrosDownloader based on its
  // specification defined in interfaces. The caller should free it after use.
  return nullptr;
}

}  // namespace ime
}  // namespace chromeos
