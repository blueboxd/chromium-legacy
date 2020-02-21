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
#include "chrome/updater/update_service.h"

@interface CRUUpdateCheckXPCServiceImpl : NSObject <CRUUpdateChecking> {
  updater::UpdateService* _service;
}

// Designated initializer.
- (instancetype)initWithUpdateService:(updater::UpdateService*)service
    NS_DESIGNATED_INITIALIZER;

@end

@implementation CRUUpdateCheckXPCServiceImpl

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
  _service->UpdateAll(base::BindOnce(
      [](base::mac::ScopedBlock<void (^)(int)> xpcReplyBlock,
         update_client::Error error) {
        VLOG(0) << "UpdateAll complete: error = " << static_cast<int>(error);
        xpcReplyBlock.get()(static_cast<int>(error));
      },
      base::mac::ScopedBlock<void (^)(int)>(reply)));
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

  _service->RegisterApp(
      request, base::BindOnce(
                   [](base::mac::ScopedBlock<void (^)(int)> xpcReplyBlock,
                      const updater::RegistrationResponse& response) {
                     VLOG(0) << "Registration complete: error = "
                             << response.status_code;
                     xpcReplyBlock.get()(response.status_code);
                   },
                   base::mac::ScopedBlock<void (^)(int)>(reply)));
}

@end

@implementation CRUUpdateCheckXPCServiceDelegate

- (instancetype)initWithUpdateService:
    (std::unique_ptr<updater::UpdateService>)service {
  if (self = [super init]) {
    _service = std::move(service);
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
  newConnection.exportedInterface =
      [NSXPCInterface interfaceWithProtocol:@protocol(CRUUpdateChecking)];
  base::scoped_nsobject<CRUUpdateCheckXPCServiceImpl> object(
      [[CRUUpdateCheckXPCServiceImpl alloc]
          initWithUpdateService:_service.get()]);
  newConnection.exportedObject = object.get();
  [newConnection resume];
  return YES;
}

@end
