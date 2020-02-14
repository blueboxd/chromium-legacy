// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_helper.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/prerender_url_loader_throttle.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace prerender {

PrerenderHelper::PrerenderHelper(content::RenderFrame* render_frame,
                                 PrerenderMode prerender_mode,
                                 const std::string& histogram_prefix)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<PrerenderHelper>(render_frame),
      prerender_mode_(prerender_mode),
      histogram_prefix_(histogram_prefix),
      start_time_(base::TimeTicks::Now()) {
  DCHECK_NE(prerender_mode_, NO_PRERENDER);
}

PrerenderHelper::~PrerenderHelper() = default;

// static
std::unique_ptr<blink::URLLoaderThrottle> PrerenderHelper::MaybeCreateThrottle(
    int render_frame_id) {
  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(render_frame_id);
  auto* prerender_helper =
      render_frame ? PrerenderHelper::Get(
                         render_frame->GetRenderView()->GetMainRenderFrame())
                   : nullptr;
  if (!prerender_helper)
    return nullptr;

  mojo::PendingRemote<chrome::mojom::PrerenderCanceler> canceler;
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      canceler.InitWithNewPipeAndPassReceiver());

  auto throttle = std::make_unique<PrerenderURLLoaderThrottle>(
      prerender_helper->prerender_mode(), prerender_helper->histogram_prefix(),
      std::move(canceler));
  prerender_helper->AddThrottle(throttle->AsWeakPtr());
  return throttle;
}

// static.
bool PrerenderHelper::IsPrerendering(const content::RenderFrame* render_frame) {
  return PrerenderHelper::GetPrerenderMode(render_frame) != NO_PRERENDER;
}

// static.
PrerenderMode PrerenderHelper::GetPrerenderMode(
    const content::RenderFrame* render_frame) {
  PrerenderHelper* helper = PrerenderHelper::Get(render_frame);
  if (!helper)
    return NO_PRERENDER;

  DCHECK_NE(helper->prerender_mode_, NO_PRERENDER);
  return helper->prerender_mode_;
}

void PrerenderHelper::DidFinishDocumentLoad() {
  if (prerender_mode_ != PREFETCH_ONLY)
    return;
  parsed_time_ = base::TimeTicks::Now();
  prefetch_finished_ = true;
  if (prefetch_count_ == 0)
    SendPrefetchFinished();
}

bool PrerenderHelper::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PrerenderHelper, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_SetIsPrerendering, OnSetIsPrerendering)
  IPC_END_MESSAGE_MAP()
  // Return false on PrerenderMsg_SetIsPrerendering so other observers can see
  // the message.
  return false;
}

void PrerenderHelper::OnDestruct() {
  delete this;
}

void PrerenderHelper::OnSetIsPrerendering(PrerenderMode mode,
                                          const std::string& histogram_prefix) {
  // Immediately after construction, |this| may receive the message that
  // triggered its creation.  If so, ignore it.
  if (mode != prerender::NO_PRERENDER)
    return;

  std::vector<base::WeakPtr<PrerenderURLLoaderThrottle>> throttles =
      std::move(throttles_);

  // |this| must be deleted so PrerenderHelper::IsPrerendering returns false
  // when the visibility state is updated, so the visibility state string will
  // not be "prerendered".
  delete this;

  for (auto& throttle : throttles) {
    if (throttle)
      throttle->PrerenderUsed();
  }
}

void PrerenderHelper::AddThrottle(
    const base::WeakPtr<PrerenderURLLoaderThrottle>& throttle) {
  // Keep track of how many pending throttles we have, as we want to defer
  // sending the "prefetch finished" signal until they are destroyed. This is
  // important since that signal tells the browser that it can tear down this
  // renderer which could interrupt subresource prefetching.
  if (prerender_mode_ == PREFETCH_ONLY) {
    prefetch_count_++;
    throttle->set_destruction_closure(base::BindOnce(
        &PrerenderHelper::OnThrottleDestroyed, weak_factory_.GetWeakPtr()));
  }
  throttles_.push_back(throttle);
}

void PrerenderHelper::OnThrottleDestroyed() {
  if (--prefetch_count_ == 0 && prefetch_finished_) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Prerender.NoStatePrefetchRendererLifetimeExtension",
        base::TimeTicks::Now() - parsed_time_);
    SendPrefetchFinished();
  }
}

void PrerenderHelper::SendPrefetchFinished() {
  DCHECK(prefetch_count_ == 0 && prefetch_finished_);
  UMA_HISTOGRAM_MEDIUM_TIMES("Prerender.NoStatePrefetchRendererParseTime",
                             parsed_time_ - start_time_);
  // TODO(darin): Perhaps this should be a routed message (frame level).
  content::RenderThread::Get()->Send(new PrerenderHostMsg_PrefetchFinished());
}

}  // namespace prerender
