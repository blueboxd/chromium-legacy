// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/network_hints_handler_impl.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace predictors {

NetworkHintsHandlerImpl::~NetworkHintsHandlerImpl() = default;

// static
void NetworkHintsHandlerImpl::Create(
    int32_t render_process_id,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new NetworkHintsHandlerImpl(render_process_id)),
      std::move(receiver));
}

void NetworkHintsHandlerImpl::PrefetchDNS(
    const std::vector<std::string>& names) {
  if (!preconnect_manager_)
    return;
  preconnect_manager_->StartPreresolveHosts(names);
}

void NetworkHintsHandlerImpl::Preconnect(int32_t render_frame_id,
                                         const GURL& url,
                                         bool allow_credentials) {
  if (!preconnect_manager_)
    return;

  if (!url.is_valid() || !url.has_host() || !url.has_scheme() ||
      !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  // TODO(mmenke):  Think about enabling cross-site preconnects, though that
  // will result in at least some cross-site information leakage.

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id);
  if (!render_frame_host)
    return;

  preconnect_manager_->StartPreconnectUrl(
      url, allow_credentials, render_frame_host->GetNetworkIsolationKey());
}

NetworkHintsHandlerImpl::NetworkHintsHandlerImpl(int32_t render_process_id)
    : render_process_id_(render_process_id) {
  // Get the PreconnectManager for this process.
  auto* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  auto* profile =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (loading_predictor && loading_predictor->preconnect_manager())
    preconnect_manager_ = loading_predictor->preconnect_manager()->GetWeakPtr();
}

}  // namespace predictors
