// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/content_settings_manager_impl.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "weblayer/browser/cookie_settings_factory.h"

namespace weblayer {

ContentSettingsManagerImpl::~ContentSettingsManagerImpl() = default;

// static
void ContentSettingsManagerImpl::Create(
    content::RenderProcessHost* render_process_host,
    mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ContentSettingsManagerImpl(render_process_host)),
      std::move(receiver));
}

void ContentSettingsManagerImpl::Clone(
    mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ContentSettingsManagerImpl(*this)),
      std::move(receiver));
}

void ContentSettingsManagerImpl::AllowStorageAccess(
    int32_t render_frame_id,
    StorageType storage_type,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(cookie_settings_->IsCookieAccessAllowed(
      origin.GetURL(), site_for_cookies, top_frame_origin));
}

void ContentSettingsManagerImpl::OnContentBlocked(int32_t render_frame_id,
                                                  ContentSettingsType type) {}

ContentSettingsManagerImpl::ContentSettingsManagerImpl(
    content::RenderProcessHost* render_process_host)
    : render_process_id_(render_process_host->GetID()),
      cookie_settings_(CookieSettingsFactory::GetForBrowserContext(
          render_process_host->GetBrowserContext())) {}

ContentSettingsManagerImpl::ContentSettingsManagerImpl(
    const ContentSettingsManagerImpl& other)
    : render_process_id_(other.render_process_id_),
      cookie_settings_(other.cookie_settings_) {}

}  // namespace weblayer
