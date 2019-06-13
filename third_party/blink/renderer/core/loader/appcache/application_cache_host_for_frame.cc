// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_frame.h"

#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

ApplicationCacheHostForFrame::ApplicationCacheHostForFrame(
    LocalFrame* local_frame,
    ApplicationCacheHostClient* client,
    const base::UnguessableToken& appcache_host_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : ApplicationCacheHostHelper(local_frame,
                                 client,
                                 appcache_host_id,
                                 std::move(task_runner)),
      local_frame_(local_frame) {}

void ApplicationCacheHostForFrame::LogMessage(
    mojom::blink::ConsoleMessageLevel log_level,
    const String& message) {
  if (WebTestSupport::IsRunningWebTest())
    return;

  if (!local_frame_ || !local_frame_->IsMainFrame())
    return;

  Frame* main_frame = local_frame_->GetPage()->MainFrame();
  if (!main_frame->IsLocalFrame())
    return;
  // TODO(michaeln): Make app cache host per-frame and correctly report to the
  // involved frame.
  auto* local_frame = DynamicTo<LocalFrame>(main_frame);
  local_frame->GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kOther, log_level, message));
}

void ApplicationCacheHostForFrame::SetSubresourceFactory(
    network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) {
  auto info = std::make_unique<URLLoaderFactoryBundleInfo>();
  info->appcache_factory_info().set_handle(
      url_loader_factory.PassInterface().PassHandle());
  local_frame_->Client()->UpdateSubresourceFactory(std::move(info));
}

void ApplicationCacheHostForFrame::Trace(blink::Visitor* visitor) {
  visitor->Trace(local_frame_);
  ApplicationCacheHostHelper::Trace(visitor);
}

}  // namespace blink
