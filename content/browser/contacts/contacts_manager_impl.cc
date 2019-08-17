// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/contacts/contacts_manager_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if defined(OS_ANDROID)
#include "content/browser/contacts/contacts_provider_android.h"
#endif

namespace content {

namespace {

std::unique_ptr<ContactsProvider> CreateProvider(
    RenderFrameHostImpl* render_frame_host) {
  if (render_frame_host->GetParent())
    return nullptr;  // This API is only supported on the main frame.
#if defined(OS_ANDROID)
  return std::make_unique<ContactsProviderAndroid>(render_frame_host);
#else
  return nullptr;
#endif
}

}  // namespace

// static
void ContactsManagerImpl::CreateForRequest(
    RenderFrameHostImpl* render_frame_host,
    blink::mojom::ContactsManagerRequest request) {
  // Implicit conversion from ContactsManagerRequest to
  // mojo::PendingReceiver<blink::mojom::ContactsManager>.
  Create(render_frame_host, std::move(request));
}

// static
void ContactsManagerImpl::Create(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ContactsManager> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ContactsManagerImpl>(render_frame_host),
      std::move(receiver));
}

ContactsManagerImpl::ContactsManagerImpl(RenderFrameHostImpl* render_frame_host)
    : contacts_provider_(CreateProvider(render_frame_host)) {}

ContactsManagerImpl::~ContactsManagerImpl() = default;

void ContactsManagerImpl::Select(bool multiple,
                                 bool include_names,
                                 bool include_emails,
                                 bool include_tel,
                                 SelectCallback callback) {
  if (contacts_provider_) {
    contacts_provider_->Select(multiple, include_names, include_emails,
                               include_tel, std::move(callback));
  } else {
    std::move(callback).Run(base::nullopt);
  }
}

}  // namespace content
