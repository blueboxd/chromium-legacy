// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/server/mac/service_delegate.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/version.h"
#import "chrome/updater/mac/xpc_service_names.h"
#import "chrome/updater/server/mac/service_protocol.h"
#import "chrome/updater/server/mac/update_service_wrappers.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_version.h"

@interface CRUUpdateCheckXPCServiceImpl
    : NSObject <CRUUpdateChecking, CRUAdministering>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializers.
- (instancetype)initWithUpdateService:(updater::UpdateService*)service

                       callbackRunner:(scoped_refptr<base::SequencedTaskRunner>)
                                          callbackRunner;
- (instancetype)initWithUpdateService:(updater::UpdateService*)service
                       callbackRunner:(scoped_refptr<base::SequencedTaskRunner>)
                                          callbackRunner
             updateCheckXPCConnection:(base::scoped_nsobject<NSXPCConnection>)
                                          updateCheckXPCConnection
    NS_DESIGNATED_INITIALIZER;

@end

@implementation CRUUpdateCheckXPCServiceImpl {
  updater::UpdateService* _service;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
  base::scoped_nsobject<NSXPCConnection> _updateCheckXPCConnection;
}

- (instancetype)initWithUpdateService:(updater::UpdateService*)service
                       callbackRunner:(scoped_refptr<base::SequencedTaskRunner>)
                                          callbackRunner {
  [self initWithUpdateService:service
                callbackRunner:callbackRunner
      updateCheckXPCConnection:base::scoped_nsobject<NSXPCConnection>(nil)];
  return self;
}

- (instancetype)initWithUpdateService:(updater::UpdateService*)service
                       callbackRunner:(scoped_refptr<base::SequencedTaskRunner>)
                                          callbackRunner
             updateCheckXPCConnection:(base::scoped_nsobject<NSXPCConnection>)
                                          updateCheckXPCConnection {
  if (self = [super init]) {
    _service = service;
    _callbackRunner = callbackRunner;
    _updateCheckXPCConnection = updateCheckXPCConnection;
  }
  return self;
}

#pragma mark CRUUpdateChecking
- (void)checkForUpdatesWithUpdateState:(id<CRUUpdateStateObserving>)updateState
                                 reply:(void (^_Nonnull)(int rc))reply {
  auto cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "UpdateAll complete: error = " << static_cast<int>(error);
        if (reply)
          reply(static_cast<int>(error));
      }));

  auto sccb = base::BindRepeating(base::RetainBlock(^(
      updater::UpdateService::UpdateState state) {
    base::scoped_nsobject<NSString> version(@"");
    if (state.next_version.IsValid()) {
      version.reset(base::SysUTF8ToNSString(state.next_version.GetString()));
    }

    base::scoped_nsobject<CRUUpdateStateWrapper> updateStateWrapper(
        [[CRUUpdateStateWrapper alloc]
              initWithAppId:base::SysUTF8ToNSString(state.app_id)
                      state:[[CRUUpdateStateStateWrapper alloc]
                                initWithUpdateStateState:state.state]
                    version:version.get()
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
                          reply:(void (^_Nonnull)(int rc))reply {
  auto cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "Update complete: error = " << static_cast<int>(error);
        if (reply)
          reply(static_cast<int>(error));
      }));

  auto sccb = base::BindRepeating(base::RetainBlock(^(
      updater::UpdateService::UpdateState state) {
    base::scoped_nsobject<NSString> version(@"");
    if (state.next_version.IsValid()) {
      version.reset(base::SysUTF8ToNSString(state.next_version.GetString()));
    }

    base::scoped_nsobject<CRUUpdateStateWrapper> updateStateWrapper(
        [[CRUUpdateStateWrapper alloc]
              initWithAppId:base::SysUTF8ToNSString(state.app_id)
                      state:[[CRUUpdateStateStateWrapper alloc]
                                initWithUpdateStateState:state.state]
                    version:version.get()
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
                              reply:(void (^_Nonnull)(int rc))reply {
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
        if (reply)
          reply(response.status_code);
      }));

  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(&updater::UpdateService::RegisterApp, _service,
                                request, std::move(cb)));
}

- (void)getUpdaterVersionWithReply:
    (void (^_Nonnull)(NSString* _Nullable version))reply {
  if (reply)
    reply(@UPDATER_VERSION_STRING);
}

- (BOOL)isUpdaterVersionLowerThanCandidateVersion:(NSString* _Nonnull)version {
  const base::Version selfVersion =
      base::Version(base::SysNSStringToUTF8(@UPDATER_VERSION_STRING));
  CHECK(selfVersion.IsValid());
  const base::Version candidateVersion =
      base::Version(base::SysNSStringToUTF8(version));
  if (candidateVersion.IsValid()) {
    return candidateVersion.CompareTo(selfVersion) > 0;
  }
  return NO;
}

- (void)haltForUpdateToVersion:(NSString* _Nonnull)candidateVersion
                         reply:(void (^_Nonnull)(bool shouldUpdate))reply {
  if (reply) {
    if ([self isUpdaterVersionLowerThanCandidateVersion:candidateVersion]) {
      // Halt the service for a long time, so that the update to the candidate
      // version can be performed. Reply YES.
      // TODO: crbug 1072061
      // Halting not implemented yet. Change to reply(YES) when we can halt.
      reply(NO);
    } else {
      reply(NO);
    }
  }
}

#pragma mark CRUAdministering
- (void)performAdminTasks {
  // Get the version of the active service.
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
  };

  auto reply = ^(NSString* _Nullable activeServiceVersionString) {
    if (activeServiceVersionString) {
      const base::Version activeServiceVersion =
          base::Version(base::SysNSStringToUTF8(activeServiceVersionString));
      CHECK(activeServiceVersion.IsValid());
      const base::Version selfVersion =
          base::Version(base::SysNSStringToUTF8(@UPDATER_VERSION_STRING));
      CHECK(selfVersion.IsValid());
      int versionComparisonResult = selfVersion.CompareTo(activeServiceVersion);

      if (versionComparisonResult > 0) {
        // If the versioned service is a higher version than the active service,
        // activate the versioned service.
        // TODO: crbug 1072061

      } else if (versionComparisonResult < 0) {
        // If the versioned service is a lower version than the active service,
        // remove the versioned service.
        // TODO: crbug 1072061
      }

      // If the versioned service is the same version as the active service, do
      // nothing.
    } else {
      // Active service vesion is nil. What now?
      // TODO: crbug 1072061
    }
  };

  if (!_updateCheckXPCConnection) {
    // Activate the service.
    // TODO: crbug 1072061
  } else {
    [[_updateCheckXPCConnection.get()
        remoteObjectProxyWithErrorHandler:errorHandler]
        getUpdaterVersionWithReply:reply];
  }
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

- (BOOL)listener:(NSXPCListener*)listener
    shouldAcceptNewConnection:(NSXPCConnection*)newConnection {
  // Check to see if the other side of the connection is "okay";
  // if not, invalidate newConnection and return NO.

  newConnection.exportedInterface = updater::GetXPCUpdateCheckingInterface();

  base::scoped_nsobject<CRUUpdateCheckXPCServiceImpl> object(
      [[CRUUpdateCheckXPCServiceImpl alloc]
          initWithUpdateService:_service.get()
                 callbackRunner:_callbackRunner.get()]);
  newConnection.exportedObject = object.get();
  [newConnection resume];
  return YES;
}

@end

@implementation CRUAdministrationXPCServiceDelegate {
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

- (BOOL)listener:(NSXPCListener*)listener
    shouldAcceptNewConnection:(NSXPCConnection*)newConnection {
  // Check to see if the other side of the connection is "okay";
  // if not, invalidate newConnection and return NO.

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  NSXPCConnectionOptions options = cmdline->HasSwitch(updater::kSystemSwitch)
                                       ? NSXPCConnectionPrivileged
                                       : 0;

  base::scoped_nsobject<NSXPCConnection> updateCheckXPCConnection(
      [[NSXPCConnection alloc]
          initWithMachServiceName:updater::GetGoogleUpdateServiceMachName()
                                      .get()
                          options:options]);

  updateCheckXPCConnection.get().remoteObjectInterface =
      updater::GetXPCUpdateCheckingInterface();

  updateCheckXPCConnection.get().interruptionHandler = ^{
    LOG(WARNING) << "CRUUpdateCheckingService: XPC connection interrupted.";
  };

  updateCheckXPCConnection.get().invalidationHandler = ^{
    LOG(WARNING) << "CRUUpdateCheckingService: XPC connection invalidated.";
  };
  [updateCheckXPCConnection resume];

  newConnection.exportedInterface = updater::GetXPCAdministeringInterface();

  base::scoped_nsobject<CRUUpdateCheckXPCServiceImpl> object(
      [[CRUUpdateCheckXPCServiceImpl alloc]
             initWithUpdateService:_service.get()
                    callbackRunner:_callbackRunner.get()
          updateCheckXPCConnection:updateCheckXPCConnection]);
  newConnection.exportedObject = object.get();
  [newConnection resume];
  return YES;
}

@end
