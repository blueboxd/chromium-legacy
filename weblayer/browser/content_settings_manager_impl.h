// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_CONTENT_SETTINGS_MANAGER_IMPL_H_
#define WEBLAYER_BROWSER_CONTENT_SETTINGS_MANAGER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"

namespace content {
class RenderProcessHost;
}

namespace content_settings {
class CookieSettings;
}

namespace weblayer {

// Called by the renderer to query storage access and notify when content is
// blocked.
class ContentSettingsManagerImpl
    : public content_settings::mojom::ContentSettingsManager {
 public:
  ~ContentSettingsManagerImpl() override;

  static void Create(
      content::RenderProcessHost* render_process_host,
      mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
          receiver);

  // mojom::ContentSettingsManager methods:
  void Clone(
      mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
          receiver) override;
  void AllowStorageAccess(int32_t render_frame_id,
                          StorageType storage_type,
                          const url::Origin& origin,
                          const GURL& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          base::OnceCallback<void(bool)> callback) override;
  void OnContentBlocked(int32_t render_frame_id,
                        ContentSettingsType type) override;

 private:
  explicit ContentSettingsManagerImpl(
      content::RenderProcessHost* render_process_host);
  ContentSettingsManagerImpl(const ContentSettingsManagerImpl& other);

  const int render_process_id_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_CONTENT_SETTINGS_MANAGER_IMPL_H_
