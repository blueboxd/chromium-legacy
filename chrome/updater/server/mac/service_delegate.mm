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
#import "chrome/updater/server/mac/service_protocol.h"
#import "chrome/updater/server/mac/update_service_wrappers.h"
#include "chrome/updater/update_service.h"

@interface CRUUpdateCheckXPCServiceImpl : NSObject <CRUUpdateChecking>

// Designated initializer.
- (instancetype)initWithUpdateService:(updater::UpdateService*)service
    NS_DESIGNATED_INITIALIZER;

@end

@implementation CRUUpdateCheckXPCServiceImpl {
  updater::UpdateService* _service;
}

- (instancetype)initWithUpdateService:(updater::UpdateService*)service {
  if (self = [super init]) {
    _service = service;
  }
  return self;
}

- (instancetype)init {
  // Unsupported, but we must override NSObject's designated initializer.
  DLOG(ERROR)
      << "Plain init method not supported for CRUUpdateCheckXPCServiceImpl.";
  return [self initWithUpdateService:nullptr];
}

- (void)checkForUpdatesWithReply:(void (^_Nullable)(int rc))reply {
  base::OnceCallback<void(updater::UpdateService::Result)> cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "UpdateAll complete: error = " << static_cast<int>(error);
        reply(static_cast<int>(error));
      }));

  _service->UpdateAll({}, base::BindOnce(std::move(cb)));
}

- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
                       priority:(CRUPriorityWrapper* _Nonnull)priority
                    updateState:(CRUUpdateStateObserver* _Nonnull)updateState
                          reply:(void (^_Nullable)(int rc))reply {
  base::OnceCallback<void(updater::UpdateService::Result)> cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "Update complete: error = " << static_cast<int>(error);
        reply(static_cast<int>(error));
      }));

  _service->Update(base::SysNSStringToUTF8(appID), [priority priority],
                   [updateState callback], base::BindOnce(std::move(cb)));
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

  base::OnceCallback<void(const updater::RegistrationResponse&)> cb =
      base::BindOnce(
          base::RetainBlock(^(const updater::RegistrationResponse& response) {
            VLOG(0) << "Registration complete: status code = "
                    << response.status_code;
            reply(response.status_code);
          }));

  _service->RegisterApp(request, base::BindOnce(std::move(cb)));
}

@end

@implementation CRUUpdateCheckXPCServiceDelegate {
  scoped_refptr<updater::UpdateService> _service;
}

- (instancetype)initWithUpdateService:
    (scoped_refptr<updater::UpdateService>)service {
  if (self = [super init]) {
    _service = service;
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
  NSXPCInterface* updateCheckingInterface =
      [NSXPCInterface interfaceWithProtocol:@protocol(CRUUpdateChecking)];
  NSXPCInterface* updateStateObservingInterface =
      [NSXPCInterface interfaceWithProtocol:@protocol(CRUUpdateStateObserving)];
  [updateCheckingInterface
       setInterface:updateStateObservingInterface
        forSelector:@selector(checkForUpdateWithAppID:
                                             priority:updateState:reply:)
      argumentIndex:2
            ofReply:NO];

  newConnection.exportedInterface = updateCheckingInterface;

  base::scoped_nsobject<CRUUpdateCheckXPCServiceImpl> object(
      [[CRUUpdateCheckXPCServiceImpl alloc]
          initWithUpdateService:_service.get()]);
  newConnection.exportedObject = object.get();
  [newConnection resume];
  return YES;
}

@end
