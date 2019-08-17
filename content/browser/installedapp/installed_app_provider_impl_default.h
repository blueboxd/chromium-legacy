// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_DEFAULT_H
#define CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_DEFAULT_H

#include <string>
#include <vector>

#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

namespace content {

// The default (no-op) implementation of the InstalledAppProvider Mojo service.
// This always returns an empty set of related applications.
class InstalledAppProviderImplDefault
    : public blink::mojom::InstalledAppProvider {
 public:
  InstalledAppProviderImplDefault();
  ~InstalledAppProviderImplDefault() override;

  // InstalledAppProvider overrides:
  void FilterInstalledApps(
      std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
      FilterInstalledAppsCallback callback) override;

  // TODO(https://crbug.com/955171): Remove this method and use Create once
  // RendererInterfaceBinders uses service_manager::BinderMap instead of
  // service_manager::BinderRegistry.
  static void CreateForRequest(
      mojo::InterfaceRequest<blink::mojom::InstalledAppProvider> request);

  static void Create(
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_DEFAULT_H
