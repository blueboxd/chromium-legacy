// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_service.h"

#include "chrome/browser/service_sandbox_type.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace speech {

constexpr base::TimeDelta kIdleProcessTimeout = base::TimeDelta::FromSeconds(5);

SpeechRecognitionService::SpeechRecognitionService(
    content::BrowserContext* context)
#if !BUILDFLAG(ENABLE_SODA)
    : context_(context)
#endif  // !BUILDFLAG(ENABLE_SODA)
{
}

SpeechRecognitionService::~SpeechRecognitionService() = default;

void SpeechRecognitionService::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  LaunchIfNotRunning();
  speech_recognition_service_->BindContext(std::move(receiver));
}

void SpeechRecognitionService::OnNetworkServiceDisconnect() {
#if !BUILDFLAG(ENABLE_SODA)
  // If the Speech On-Device API is not enabled, pass the URL loader factory to
  // the speech recognition service to allow network requests to the Open Speech
  // API.
  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_trusted = false;
  params->automatically_assign_isolation_info = true;
  network::mojom::NetworkContext* network_context =
      content::BrowserContext::GetDefaultStoragePartition(context_)
          ->GetNetworkContext();
  network_context->CreateURLLoaderFactory(
      url_loader_factory.InitWithNewPipeAndPassReceiver(), std::move(params));
  speech_recognition_service_->SetUrlLoaderFactory(
      std::move(url_loader_factory));
#endif  // !BUILDFLAG(ENABLE_SODA)
}

void SpeechRecognitionService::LaunchIfNotRunning() {
  if (speech_recognition_service_.is_bound())
    return;

  content::ServiceProcessHost::Launch(
      speech_recognition_service_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_SPEECH_RECOGNITION_SERVICE_NAME)
          .Pass());

  // Ensure that if the interface is ever disconnected (e.g. the service
  // process crashes) or goes idle for a short period of time -- meaning there
  // are no in-flight messages and no other interfaces bound through this
  // one -- then we will reset |remote|, causing the service process to be
  // terminated if it isn't already.
  speech_recognition_service_.reset_on_disconnect();
  speech_recognition_service_.reset_on_idle_timeout(kIdleProcessTimeout);

  speech_recognition_service_client_.reset();
  speech_recognition_service_->BindSpeechRecognitionServiceClient(
      speech_recognition_service_client_.BindNewPipeAndPassRemote());
  OnNetworkServiceDisconnect();
}
}  // namespace speech
