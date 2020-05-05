// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/server/mac/service_delegate.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "chrome/updater/server/mac/service_protocol.h"
#import "chrome/updater/server/mac/update_service_wrappers.h"
#include "chrome/updater/update_service.h"

@interface CRUUpdateCheckXPCServiceImpl : NSObject <CRUUpdateChecking>

// Designated initializer.
- (instancetype)initWithUpdateService:(updater::UpdateService*)service
                       callbackRunner:(scoped_refptr<base::SequencedTaskRunner>)
                                          callbackRunner
    NS_DESIGNATED_INITIALIZER;

@end

@implementation CRUUpdateCheckXPCServiceImpl {
  updater::UpdateService* _service;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)initWithUpdateService:(updater::UpdateService*)service
                       callbackRunner:(scoped_refptr<base::SequencedTaskRunner>)
                                          callbackRunner {
  if (self = [super init]) {
    _service = service;
    _callbackRunner = callbackRunner;
  }
  return self;
}

- (instancetype)init {
  // Unsupported, but we must override NSObject's designated initializer.
  DLOG(ERROR)
      << "Plain init method not supported for CRUUpdateCheckXPCServiceImpl.";
  return [self initWithUpdateService:nullptr callbackRunner:nullptr];
}

- (void)checkForUpdatesWithUpdateState:(id<CRUUpdateStateObserving>)updateState
                                 reply:(void (^_Nullable)(int rc))reply {
  auto cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "UpdateAll complete: error = " << static_cast<int>(error);
        reply(static_cast<int>(error));
      }));

  auto sccb = base::BindRepeating(
      base::RetainBlock(^(updater::UpdateService::UpdateState state) {
        base::scoped_nsobject<CRUUpdateStateWrapper> updateStateWrapper(
            [[CRUUpdateStateWrapper alloc]
                  initWithAppId:base::SysUTF8ToNSString(state.app_id)
                          state:[[CRUUpdateStateStateWrapper alloc]
                                    initWithUpdateStateState:state.state]
                        version:base::SysUTF8ToNSString(
                                    state.next_version.GetString())
                downloadedBytes:state.downloaded_bytes
                     totalBytes:state.total_bytes
                installProgress:state.install_progress
                  errorCategory:[[CRUErrorCategoryWrapper alloc]
                                    initWithErrorCategory:state.error_category]
                      errorCode:state.error_code
                      extraCode:state.extra_code1]);
        [updateState observeUpdateState:updateStateWrapper.get()];
      }));

  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(&updater::UpdateService::UpdateAll, _service,
                                std::move(sccb), std::move(cb)));
}

- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
                       priority:(CRUPriorityWrapper* _Nonnull)priority
                    updateState:(id<CRUUpdateStateObserving>)updateState
                          reply:(void (^_Nullable)(int rc))reply {
  auto cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "Update complete: error = " << static_cast<int>(error);
        reply(static_cast<int>(error));
      }));

  auto sccb = base::BindRepeating(
      base::RetainBlock(^(updater::UpdateService::UpdateState state) {
        base::scoped_nsobject<CRUUpdateStateWrapper> updateStateWrapper(
            [[CRUUpdateStateWrapper alloc]
                  initWithAppId:base::SysUTF8ToNSString(state.app_id)
                          state:[[CRUUpdateStateStateWrapper alloc]
                                    initWithUpdateStateState:state.state]
                        version:base::SysUTF8ToNSString(
                                    state.next_version.GetString())
                downloadedBytes:state.downloaded_bytes
                     totalBytes:state.total_bytes
                installProgress:state.install_progress
                  errorCategory:[[CRUErrorCategoryWrapper alloc]
                                    initWithErrorCategory:state.error_category]
                      errorCode:state.error_code
                      extraCode:state.extra_code1]);
        [updateState observeUpdateState:updateStateWrapper.get()];
      }));

  _callbackRunner->PostTask(
      FROM_HERE,
      base::BindOnce(&updater::UpdateService::Update, _service,
                     base::SysNSStringToUTF8(appID), [priority priority],
                     std::move(sccb), std::move(cb)));
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                                tag:(NSString* _Nullable)tag
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nullable)(int rc))reply {
  updater::RegistrationRequest request;
  request.app_id = base::SysNSStringToUTF8(appId);
  request.brand_code = base::SysNSStringToUTF8(brandCode);
  request.tag = base::SysNSStringToUTF8(tag);
  request.version = base::Version(base::SysNSStringToUTF8(version));
  request.existence_checker_path =
      base::FilePath(base::SysNSStringToUTF8(existenceCheckerPath));

  auto cb = base::BindOnce(
      base::RetainBlock(^(const updater::RegistrationResponse& response) {
        VLOG(0) << "Registration complete: status code = "
                << response.status_code;
        reply(response.status_code);
      }));

  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(&updater::UpdateService::RegisterApp, _service,
                                request, std::move(cb)));
}

@end

@implementation CRUUpdateCheckXPCServiceDelegate {
  scoped_refptr<updater::UpdateService> _service;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)initWithUpdateService:
    (scoped_refptr<updater::UpdateService>)service {
  if (self = [super init]) {
    _service = service;
    _callbackRunner = base::SequencedTaskRunnerHandle::Get();
  }
  return self;
}

- (instancetype)init {
  // Unsupported, but we must override NSObject's designated initializer.
  DLOG(ERROR) << "Plain init method not supported for "
                 "CRUUpdateCheckXPCServiceDelegate.";
  return [self initWithUpdateService:nullptr];
}

- (BOOL)listener:(NSXPCListener*)listener
    shouldAcceptNewConnection:(NSXPCConnection*)newConnection {
  // Check to see if the other side of the connection is "okay";
  // if not, invalidate newConnection and return NO.

  newConnection.exportedInterface = updater::GetXpcInterface();

  base::scoped_nsobject<CRUUpdateCheckXPCServiceImpl> object(
      [[CRUUpdateCheckXPCServiceImpl alloc]
          initWithUpdateService:_service.get()
                 callbackRunner:_callbackRunner.get()]);
  newConnection.exportedObject = object.get();
  [newConnection resume];
  return YES;
}

@end
