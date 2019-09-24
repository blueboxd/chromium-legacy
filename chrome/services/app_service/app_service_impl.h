// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_
#define CHROME_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_

#include <map>

#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace apps {

// The implementation of the apps::mojom::AppService Mojo interface. For the
// service (in the service_manager::Service sense) aspect of the App Service,
// see the AppService class.
//
// See chrome/services/app_service/README.md.
class AppServiceImpl : public apps::mojom::AppService {
 public:
  AppServiceImpl();
  ~AppServiceImpl() override;

  void BindReceiver(mojo::PendingReceiver<apps::mojom::AppService> receiver);

  // apps::mojom::AppService overrides.
  void RegisterPublisher(
      mojo::PendingRemote<apps::mojom::Publisher> publisher_remote,
      apps::mojom::AppType app_type) override;
  void RegisterSubscriber(apps::mojom::SubscriberPtr subscriber,
                          apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(apps::mojom::AppType app_type,
                const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(apps::mojom::AppType app_type,
              const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void SetPermission(apps::mojom::AppType app_type,
                     const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(apps::mojom::AppType app_type,
                 const std::string& app_id) override;
  void OpenNativeSettings(apps::mojom::AppType app_type,
                          const std::string& app_id) override;

 private:
  void OnPublisherDisconnected(apps::mojom::AppType app_type);

  // publishers_ is a std::map, not a mojo::RemoteSet, since we want to
  // be able to find *the* publisher for a given apps::mojom::AppType.
  std::map<apps::mojom::AppType, mojo::Remote<apps::mojom::Publisher>>
      publishers_;
  mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_;

  // Must come after the publisher and subscriber maps to ensure it is
  // destroyed first, closing the connection to avoid dangling callbacks.
  mojo::ReceiverSet<apps::mojom::AppService> receivers_;

  DISALLOW_COPY_AND_ASSIGN(AppServiceImpl);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_
