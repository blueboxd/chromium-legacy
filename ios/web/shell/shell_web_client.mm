// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/shell_web_client.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "ios/web/public/service/service_names.mojom.h"
#include "ios/web/public/user_agent.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/shell/shell_web_main_parts.h"
#import "ios/web/shell/web_usage_controller.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/test/user_id/public/cpp/manifest.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// Implementation of mojom::WebUsageController that exposes the ability
// to set whether a WebState has web usage enabled via Mojo.
class WebUsageController : public mojom::WebUsageController {
 public:
  explicit WebUsageController(WebState* web_state) : web_state_(web_state) {}
  ~WebUsageController() override {}

  static void Create(mojo::InterfaceRequest<mojom::WebUsageController> request,
                     WebState* web_state) {
    mojo::MakeStrongBinding(std::make_unique<WebUsageController>(web_state),
                            std::move(request));
  }

 private:
  void SetWebUsageEnabled(bool enabled,
                          SetWebUsageEnabledCallback callback) override {
    web_state_->SetWebUsageEnabled(enabled);
    std::move(callback).Run();
  }

  WebState* web_state_;
};

}  // namespace

ShellWebClient::ShellWebClient() : web_main_parts_(nullptr) {}

ShellWebClient::~ShellWebClient() {
}

std::unique_ptr<web::WebMainParts> ShellWebClient::CreateWebMainParts() {
  auto web_main_parts = std::make_unique<ShellWebMainParts>();
  web_main_parts_ = web_main_parts.get();
  return web_main_parts;
}

ShellBrowserState* ShellWebClient::browser_state() const {
  return web_main_parts_->browser_state();
}

std::string ShellWebClient::GetUserAgent(UserAgentType type) const {
  return web::BuildUserAgentFromProduct("CriOS/36.77.34.45");
}

base::StringPiece ShellWebClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ShellWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

bool ShellWebClient::IsDataResourceGzipped(int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().IsGzipped(resource_id);
}

base::Optional<service_manager::Manifest>
ShellWebClient::GetServiceManifestOverlay(base::StringPiece name) {
  if (name == mojom::kBrowserServiceName) {
    return service_manager::ManifestBuilder()
        .RequireCapability("user_id", "user_id")
        .PackageService(user_id::GetManifest())
        .Build();
  }

  return base::nullopt;
}

void ShellWebClient::BindInterfaceRequestFromMainFrame(
    WebState* web_state,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (!main_frame_interfaces_.get() &&
      !main_frame_interfaces_parameterized_.get()) {
    InitMainFrameInterfaces();
  }

  if (!main_frame_interfaces_parameterized_->TryBindInterface(
          interface_name, &interface_pipe, web_state)) {
    main_frame_interfaces_->TryBindInterface(interface_name, &interface_pipe);
  }
}

void ShellWebClient::AllowCertificateError(
    WebState*,
    int /*cert_error*/,
    const net::SSLInfo&,
    const GURL&,
    bool overridable,
    const base::Callback<void(bool)>& callback) {
  base::Callback<void(bool)> block_callback(callback);
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:@"Your connection is not private"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  [alert addAction:[UIAlertAction actionWithTitle:@"Go Back"
                                            style:UIAlertActionStyleCancel
                                          handler:^(UIAlertAction*) {
                                            block_callback.Run(false);
                                          }]];

  if (overridable) {
    [alert addAction:[UIAlertAction actionWithTitle:@"Continue"
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction*) {
                                              block_callback.Run(true);
                                            }]];
  }
  [[UIApplication sharedApplication].keyWindow.rootViewController
      presentViewController:alert
                   animated:YES
                 completion:nil];
}

void ShellWebClient::InitMainFrameInterfaces() {
  main_frame_interfaces_ = std::make_unique<service_manager::BinderRegistry>();
  main_frame_interfaces_parameterized_ =
      std::make_unique<service_manager::BinderRegistryWithArgs<WebState*>>();
  main_frame_interfaces_parameterized_->AddInterface(
      base::Bind(WebUsageController::Create));
}

}  // namespace web
