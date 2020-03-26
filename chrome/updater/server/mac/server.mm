// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/server/mac/server.h"

#import <Foundation/Foundation.h>
#include <xpc/xpc.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "chrome/updater/app/app.h"
#import "chrome/updater/configurator.h"
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
  base::scoped_nsobject<CRUUpdateCheckXPCServiceDelegate> delegate_;
  base::scoped_nsobject<NSXPCListener> listener_;
};

AppServer::AppServer() = default;
AppServer::~AppServer() = default;

void AppServer::Initialize() {
  config_ = base::MakeRefCounted<Configurator>();
}

void AppServer::FirstTaskRun() {
  @autoreleasepool {
    delegate_.reset([[CRUUpdateCheckXPCServiceDelegate alloc]
        initWithUpdateService:base::MakeRefCounted<UpdateServiceInProcess>(
                                  config_)]);

    listener_.reset([[NSXPCListener alloc]
        initWithMachServiceName:GetGoogleUpdateServiceMachName().get()]);
    listener_.get().delegate = delegate_.get();

    [listener_ resume];
  }
}

scoped_refptr<App> AppServerInstance() {
  return AppInstance<AppServer>();
}

}  // namespace updater
