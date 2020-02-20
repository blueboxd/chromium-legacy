// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_hints_agent.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace subresource_redirect {

SubresourceRedirectHintsAgent::SubresourceRedirectHintsAgent() = default;
SubresourceRedirectHintsAgent::~SubresourceRedirectHintsAgent() = default;

void SubresourceRedirectHintsAgent::DidStartNavigation() {
  // Clear the hints when a navigation starts, so that hints from previous
  // navigation do not apply in case the same renderframe is reused.
  public_image_urls_.clear();
  public_image_urls_received_ = false;
}

void SubresourceRedirectHintsAgent::SetCompressPublicImagesHints(
    blink::mojom::CompressPublicImagesHintsPtr images_hints) {
  DCHECK(public_image_urls_.empty());
  DCHECK(!public_image_urls_received_);
  public_image_urls_ = images_hints->image_urls;
  public_image_urls_received_ = true;
}

SubresourceRedirectHintsAgent::RedirectResult
SubresourceRedirectHintsAgent::ShouldRedirectImage(const GURL& url) const {
  if (!public_image_urls_received_) {
    return RedirectResult::kIneligibleImageHintsUnavailable;
  }

  GURL::Replacements rep;
  rep.ClearRef();
  // TODO(rajendrant): Skip redirection if the URL contains username or password
  if (public_image_urls_.find(url.ReplaceComponents(rep).spec()) !=
      public_image_urls_.end()) {
    return RedirectResult::kRedirectable;
  }

  return RedirectResult::kIneligibleMissingInImageHints;
}

void SubresourceRedirectHintsAgent::RecordMetrics(
    int render_frame_id,
    int64_t content_length,
    RedirectResult redirect_result) const {
  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(render_frame_id);
  if (!render_frame || !render_frame->GetWebFrame())
    return;

  ukm::builders::PublicImageCompressionDataUse
      public_image_compression_data_use(
          render_frame->GetWebFrame()->GetDocument().GetUkmSourceId());
  content_length = ukm::GetExponentialBucketMin(content_length, 1.3);

  switch (redirect_result) {
    case RedirectResult::kRedirectable:
      public_image_compression_data_use.SetCompressibleImageBytes(
          content_length);
      break;
    case RedirectResult::kIneligibleImageHintsUnavailable:
      public_image_compression_data_use.SetIneligibleImageHintsUnavailableBytes(
          content_length);
      break;
    case RedirectResult::kIneligibleMissingInImageHints:
      public_image_compression_data_use.SetIneligibleMissingInImageHintsBytes(
          content_length);
      break;
    case RedirectResult::kIneligibleOtherImage:
      public_image_compression_data_use.SetIneligibleOtherImageBytes(
          content_length);
      break;
  }
  mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> recorder;
  content::RenderThread::Get()->BindHostReceiver(
      recorder.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder =
      std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));
  public_image_compression_data_use.Record(ukm_recorder.get());
}

}  // namespace subresource_redirect
