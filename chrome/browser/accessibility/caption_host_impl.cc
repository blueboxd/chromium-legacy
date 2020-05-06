// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_host_impl.h"

#include <memory>
#include <utility>

#include "chrome/browser/accessibility/caption_controller.h"
#include "chrome/browser/accessibility/caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace captions {

// static
void CaptionHostImpl::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<chrome::mojom::CaptionHost> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<CaptionHostImpl>(frame_host),
                              std::move(receiver));
}

CaptionHostImpl::CaptionHostImpl(content::RenderFrameHost* frame_host)
    : frame_host_(frame_host) {
  if (!frame_host_)
    return;
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host_);
  if (!web_contents) {
    frame_host_ = nullptr;
    return;
  }
  Observe(web_contents);
}

CaptionHostImpl::~CaptionHostImpl() = default;

void CaptionHostImpl::OnTranscription(
    chrome::mojom::TranscriptionResultPtr transcription_result) {
  if (!frame_host_)
    return;
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host_);
  if (!web_contents) {
    frame_host_ = nullptr;
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return;
  CaptionControllerFactory::GetForProfile(profile)->DispatchTranscription(
      web_contents, transcription_result);
}

void CaptionHostImpl::RenderFrameDeleted(content::RenderFrameHost* frame_host) {
  if (frame_host == frame_host_)
    frame_host_ = nullptr;
}

}  // namespace captions
