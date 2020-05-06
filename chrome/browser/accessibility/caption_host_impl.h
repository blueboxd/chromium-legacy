// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_CAPTION_HOST_IMPL_H_
#define CHROME_BROWSER_ACCESSIBILITY_CAPTION_HOST_IMPL_H_

#include <string>

#include "chrome/common/caption.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Caption Host Impl
//
//  A class that implements the Mojo interface CaptionHost. There exists one
//  CaptionHostImpl per render frame.
//
class CaptionHostImpl : public chrome::mojom::CaptionHost,
                        public content::WebContentsObserver {
 public:
  explicit CaptionHostImpl(content::RenderFrameHost* frame_host);
  CaptionHostImpl(const CaptionHostImpl&) = delete;
  CaptionHostImpl& operator=(const CaptionHostImpl&) = delete;
  ~CaptionHostImpl() override;

  // static
  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<chrome::mojom::CaptionHost> receiver);

  // chrome::mojom::CaptionHost:
  void OnTranscription(
      chrome::mojom::TranscriptionResultPtr transcription_result) override;

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* frame_host) override;

 private:
  content::RenderFrameHost* frame_host_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_CAPTION_HOST_IMPL_H_
