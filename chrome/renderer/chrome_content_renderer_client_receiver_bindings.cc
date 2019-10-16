// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services in the renderer to the browser.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/renderer/media/webrtc_logging_agent_impl.h"
#include "components/safe_browsing/buildflags.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#endif

#if defined(OS_LINUX)
#include "base/allocator/buildflags.h"
#if BUILDFLAG(USE_TCMALLOC)
#include "chrome/common/performance_manager/mojom/tcmalloc.mojom.h"
#include "chrome/renderer/performance_manager/mechanisms/tcmalloc_tunables_impl.h"
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // defined(OS_LINUX)

void ChromeContentRendererClient::BindReceiverOnMainThread(
    mojo::GenericPendingReceiver receiver) {
  if (auto agent_receiver = receiver.As<chrome::mojom::WebRtcLoggingAgent>()) {
    if (!webrtc_logging_agent_impl_) {
      webrtc_logging_agent_impl_ =
          std::make_unique<chrome::WebRtcLoggingAgentImpl>();
    }
    webrtc_logging_agent_impl_->AddReceiver(std::move(agent_receiver));
    return;
  }

#if BUILDFLAG(FULL_SAFE_BROWSING)
  if (auto setter_receiver =
          receiver.As<safe_browsing::mojom::PhishingModelSetter>()) {
    safe_browsing::PhishingClassifierFilter::Create(std::move(setter_receiver));
    return;
  }
#endif

#if defined(OS_LINUX)
#if BUILDFLAG(USE_TCMALLOC)
  if (auto setter_receiver = receiver.As<tcmalloc::mojom::TcmallocTunables>()) {
    performance_manager::mechanism::TcmallocTunablesImpl::Create(
        std::move(setter_receiver));
    return;
  }
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // defined(OS_LINUX)

  // TODO(crbug.com/977637): Get rid of the use of BinderRegistry here. This was
  // done only to avoid churning spellcheck code while eliminting the "chrome"
  // and "chrome_renderer" services. Spellcheck is (and should remain) the only
  // user of |registry_|.
  std::string interface_name = *receiver.interface_name();
  auto pipe = receiver.PassPipe();
  registry_.TryBindInterface(interface_name, &pipe);
}
