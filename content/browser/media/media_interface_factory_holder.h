// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_FACTORY_HOLDER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_FACTORY_HOLDER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

namespace content {

// Helper class to get mojo::PendingRemote<media::mojom::InterfaceFactory>.
// Get() lazily connects to the global Media Service instance.
class MediaInterfaceFactoryHolder {
 public:
  using MediaServiceGetter =
      base::RepeatingCallback<media::mojom::MediaService&()>;
  using CreateInterfaceProviderCB = base::RepeatingCallback<
      mojo::PendingRemote<service_manager::mojom::InterfaceProvider>()>;

  // |media_service_getter| will be called from the UI thread.
  MediaInterfaceFactoryHolder(
      MediaServiceGetter media_service_getter,
      CreateInterfaceProviderCB create_interface_provider_cb);
  ~MediaInterfaceFactoryHolder();

  // Gets the MediaService |interface_factory_remote_|. The returned pointer is
  // still owned by this class.
  media::mojom::InterfaceFactory* Get();

 private:
  void ConnectToMediaService();

  // Callback for connection error from |interface_factory_remote_|.
  void OnMediaServiceConnectionError();

  MediaServiceGetter media_service_getter_;
  CreateInterfaceProviderCB create_interface_provider_cb_;
  mojo::Remote<media::mojom::InterfaceFactory> interface_factory_remote_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(MediaInterfaceFactoryHolder);
};

}  // namespace content
#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_FACTORY_HOLDER_H_
