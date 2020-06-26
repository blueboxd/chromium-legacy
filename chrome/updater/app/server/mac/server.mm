// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/server/mac/server.h"

#import <Foundation/Foundation.h>
#include <xpc/xpc.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_server.h"
#import "chrome/updater/app/server/mac/app_server.h"
#include "chrome/updater/app/server/mac/service_delegate.h"
#include "chrome/updater/configurator.h"
#import "chrome/updater/mac/setup/info_plist.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service_in_process.h"

namespace updater {

AppServerMac::AppServerMac() = default;
AppServerMac::~AppServerMac() = default;

void AppServerMac::Uninitialize() {
  // These delegates need to have a reference to the AppServer. To break the
  // circular reference, we need to reset them.
  update_check_delegate_.reset();
  administration_delegate_.reset();

  AppServer::Uninitialize();
}

void AppServerMac::ActiveDuty() {
  @autoreleasepool {
    // Sets up a listener and delegate for the CRUUpdateChecking XPC
    // connection
    update_check_delegate_.reset([[CRUUpdateCheckXPCServiceDelegate alloc]
        initWithUpdateService:base::MakeRefCounted<UpdateServiceInProcess>(
                                  config_)
                    appServer:scoped_refptr<AppServerMac>(this)]);

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
                                  config_)
                    appServer:scoped_refptr<AppServerMac>(this)]);

    administration_listener_.reset([[NSXPCListener alloc]
        initWithMachServiceName:
            base::mac::CFToNSCast(
                info_plist->GoogleUpdateServiceLaunchdNameVersioned().get())]);
    administration_listener_.get().delegate = administration_delegate_.get();

    [administration_listener_ resume];
  }
}

void AppServerMac::UninstallSelf() {
  // TODO(crbug.com/1098934): Uninstall this candidate version of the updater.
}

bool AppServerMac::SwapRPCInterfaces() {
  // TODO(crbug.com/1098935): Update unversioned XPC interface.
  return true;
}

void AppServerMac::TaskStarted() {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&AppServerMac::MarkTaskStarted, this));
}

void AppServerMac::MarkTaskStarted() {
  tasks_running_++;
}

void AppServerMac::TaskCompleted() {
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AppServerMac::AcknowledgeTaskCompletion, this),
      base::TimeDelta::FromSeconds(10));
}

void AppServerMac::AcknowledgeTaskCompletion() {
  if (--tasks_running_ < 1) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppServerMac::Shutdown, this, 0));
  }
}

scoped_refptr<App> MakeAppServer() {
  return base::MakeRefCounted<AppServerMac>();
}

}  // namespace updater
