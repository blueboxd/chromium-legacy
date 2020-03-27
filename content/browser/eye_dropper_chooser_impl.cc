// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/eye_dropper_chooser_impl.h"

#include "base/callback.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"

namespace content {

// static
void EyeDropperChooserImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::EyeDropperChooser> receiver) {
  DCHECK(render_frame_host);
  new EyeDropperChooserImpl(render_frame_host, std::move(receiver));
}

EyeDropperChooserImpl::EyeDropperChooserImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::EyeDropperChooser> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)) {}

void EyeDropperChooserImpl::Choose(ChooseCallback callback) {
  // TODO(crbug.com/992297): add browser side support for eye dropper.
  std::move(callback).Run(false, 0);
}

}  // namespace content