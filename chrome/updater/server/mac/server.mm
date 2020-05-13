// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/server/mac/server.h"

#import <Foundation/Foundation.h>
#include <xpc/xpc.h>

#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "chrome/updater/app/app.h"
#import "chrome/updater/configurator.h"
#import "chrome/updater/mac/setup/info_plist.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/server/mac/service_delegate.h"
#include "chrome/updater/update_service_in_process.h"

namespace updater {

class AppServer : public App {
 public:
  AppServer();

 private:
  ~AppServer() override;
  void Initialize() override;
  void FirstTaskRun() override;

  scoped_refptr<Configurator> config_;
  base::scoped_nsobject<CRUUpdateCheckXPCServiceDelegate>
      update_check_delegate_;
  base::scoped_nsobject<NSXPCListener> update_check_listener_;
  base::scoped_nsobject<CRUAdministrationXPCServiceDelegate>
      administration_delegate_;
  base::scoped_nsobject<NSXPCListener> administration_listener_;
};

AppServer::AppServer() = default;
AppServer::~AppServer() = default;

void AppServer::Initialize() {
  config_ = base::MakeRefCounted<Configurator>();
}

void AppServer::FirstTaskRun() {
  @autoreleasepool {
    // Sets up a listener and delegate for the CRUUpdateChecking XPC connection
    update_check_delegate_.reset([[CRUUpdateCheckXPCServiceDelegate alloc]
        initWithUpdateService:base::MakeRefCounted<UpdateServiceInProcess>(
                                  config_)]);

    update_check_listener_.reset([[NSXPCListener alloc]
        initWithMachServiceName:GetGoogleUpdateServiceMachName().get()]);
    update_check_listener_.get().delegate = update_check_delegate_.get();

    [update_check_listener_ resume];

    // Sets up a listener and delegate for the CRUAdministering XPC connection
    const std::unique_ptr<InfoPlist> info_plist =
        InfoPlist::Create(InfoPlistPath());
    CHECK(info_plist);
    administration_delegate_.reset([[CRUAdministrationXPCServiceDelegate alloc]
        initWithUpdateService:base::MakeRefCounted<UpdateServiceInProcess>(
                                  config_)]);

    administration_listener_.reset([[NSXPCListener alloc]
        initWithMachServiceName:
            base::mac::CFToNSCast(
                info_plist->GoogleUpdateServiceLaunchdNameVersioned().get())]);
    administration_listener_.get().delegate = administration_delegate_.get();

    [administration_listener_ resume];
  }
}

scoped_refptr<App> AppServerInstance() {
  return AppInstance<AppServer>();
}

}  // namespace updater
