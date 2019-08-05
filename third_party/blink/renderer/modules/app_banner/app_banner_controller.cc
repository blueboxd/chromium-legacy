// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/app_banner/app_banner_controller.h"

#include <memory>
#include <utility>
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/app_banner/before_install_prompt_event.h"

namespace blink {

AppBannerController::AppBannerController(LocalFrame& frame) : frame_(frame) {}

void AppBannerController::BindMojoRequest(
    LocalFrame* frame,
    mojom::blink::AppBannerControllerRequest request) {
  DCHECK(frame);

  // See https://bit.ly/2S0zRAS for task types.
  mojo::MakeStrongBinding(std::make_unique<AppBannerController>(*frame),
                          std::move(request),
                          frame->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

void AppBannerController::BannerPromptRequest(
    mojom::blink::AppBannerServicePtr service_ptr,
    mojom::blink::AppBannerEventRequest event_request,
    const Vector<String>& platforms,
    BannerPromptRequestCallback callback) {
  // TODO(hajimehoshi): Add tests for the case the frame is detached.
  if (!frame_ || !frame_->GetDocument() || !frame_->IsAttached()) {
    std::move(callback).Run(mojom::blink::AppBannerPromptReply::NONE);
    return;
  }

  mojom::AppBannerPromptReply reply =
      frame_->DomWindow()->DispatchEvent(*BeforeInstallPromptEvent::Create(
          event_type_names::kBeforeinstallprompt, *frame_,
          std::move(service_ptr), std::move(event_request), platforms)) ==
              DispatchEventResult::kNotCanceled
          ? mojom::AppBannerPromptReply::NONE
          : mojom::AppBannerPromptReply::CANCEL;

  std::move(callback).Run(reply);
}

}  // namespace blink
