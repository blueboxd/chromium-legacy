// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/worker_shadow_page.h"

#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/document_interface_broker.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"

namespace blink {

namespace {

constexpr char kDoNotTrackHeader[] = "DNT";

}  // namespace

mojo::ScopedMessagePipeHandle CreateStubDocumentInterfaceBrokerHandle() {
  mojom::blink::DocumentInterfaceBrokerPtrInfo info;
  return mojo::MakeRequest(&info).PassMessagePipe();
}

WorkerShadowPage::WorkerShadowPage(
    Client* client,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    PrivacyPreferences preferences,
    const base::UnguessableToken& appcache_host_id)
    : client_(client),
      web_view_(WebViewImpl::Create(nullptr,
                                    /*is_hidden=*/false,
                                    /*compositing_enabled=*/false,
                                    nullptr)),
      main_frame_(WebLocalFrameImpl::CreateMainFrame(
          web_view_,
          this,
          nullptr /* interface_registry */,
          CreateStubDocumentInterfaceBrokerHandle(),
          nullptr /* opener */,
          g_empty_atom,
          WebSandboxFlags::kNone,
          FeaturePolicy::FeatureState())),
      loader_factory_(std::move(loader_factory)),
      appcache_host_id_(appcache_host_id),
      preferences_(std::move(preferences)) {
  DCHECK(IsMainThread());

  main_frame_->SetDevToolsAgentImpl(
      WebDevToolsAgentImpl::CreateForWorker(main_frame_, client_));
}

WorkerShadowPage::~WorkerShadowPage() {
  DCHECK(IsMainThread());
  // Detach the client before closing the view to avoid getting called back.
  main_frame_->SetClient(nullptr);
  web_view_->MainFrameWidget()->Close();
  main_frame_->Close();
}

void WorkerShadowPage::Initialize(const KURL& script_url) {
  DCHECK(IsMainThread());
  AdvanceState(State::kInitializing);

  // Construct substitute data source. We only need it to have same origin as
  // the worker so the loading checks work correctly.
  std::string content("");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(
          SharedBuffer::Create(content.c_str(), content.length()), script_url);
  params->appcache_host_id = appcache_host_id_;
  main_frame_->GetFrame()->Loader().CommitNavigation(std::move(params),
                                                     nullptr /* extra_data */);
}

void WorkerShadowPage::DidFinishDocumentLoad() {
  DCHECK(IsMainThread());
  AdvanceState(State::kInitialized);
  client_->OnShadowPageInitialized();
}

std::unique_ptr<blink::WebURLLoaderFactory>
WorkerShadowPage::CreateURLLoaderFactory() {
  DCHECK(IsMainThread());
  if (loader_factory_)
    return Platform::Current()->WrapSharedURLLoaderFactory(loader_factory_);
  return Platform::Current()->CreateDefaultURLLoaderFactory();
}

base::UnguessableToken WorkerShadowPage::GetDevToolsFrameToken() {
  // TODO(dgozman): instrumentation token will have to be passed directly to
  // DevTools once we stop using a frame for workers. Currently, we rely on
  // the frame's instrumentation token to match the worker.
  return client_->GetDevToolsWorkerToken();
}

void WorkerShadowPage::WillSendRequest(WebURLRequest& request) {
  if (preferences_.enable_do_not_track) {
    request.SetHttpHeaderField(WebString::FromUTF8(kDoNotTrackHeader), "1");
  }
  if (!preferences_.enable_referrers) {
    request.SetHttpReferrer(WebString(),
                            network::mojom::ReferrerPolicy::kDefault);
  }
}

void WorkerShadowPage::BeginNavigation(
    std::unique_ptr<WebNavigationInfo> info) {
  NOTREACHED();
}

bool WorkerShadowPage::WasInitialized() const {
  return state_ == State::kInitialized;
}

void WorkerShadowPage::AdvanceState(State new_state) {
  switch (new_state) {
    case State::kUninitialized:
      NOTREACHED();
      return;
    case State::kInitializing:
      DCHECK_EQ(State::kUninitialized, state_);
      state_ = new_state;
      return;
    case State::kInitialized:
      DCHECK_EQ(State::kInitializing, state_);
      state_ = new_state;
      return;
  }
}

WebDevToolsAgentImpl* WorkerShadowPage::DevToolsAgent() {
  return main_frame_->DevToolsAgentImpl();
}

}  // namespace blink
