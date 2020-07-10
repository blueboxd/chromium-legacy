// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/server/mac/service_delegate.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/version.h"
#import "chrome/updater/app/server/mac/app_server.h"
#import "chrome/updater/app/server/mac/server.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#import "chrome/updater/app/server/mac/update_service_wrappers.h"
#include "chrome/updater/mac/setup/setup.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_version.h"

const NSInteger kMaxRedialAttempts = 3;

const base::Version& GetSelfVersion() {
  static base::NoDestructor<base::Version> self_version(
      base::Version(UPDATER_VERSION_STRING));
  return *self_version;
}

@interface CRUUpdateCheckXPCServiceImpl
    : NSObject <CRUUpdateChecking, CRUAdministering>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializers.
- (instancetype)
    initWithUpdateService:(updater::UpdateService*)service
                appServer:(scoped_refptr<updater::AppServerMac>)appServer
           callbackRunner:
               (scoped_refptr<base::SequencedTaskRunner>)callbackRunner
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
       initWithUpdateService:(updater::UpdateService*)service
                   appServer:(scoped_refptr<updater::AppServerMac>)appServer
    updaterConnectionOptions:(NSXPCConnectionOptions)options
              callbackRunner:
                  (scoped_refptr<base::SequencedTaskRunner>)callbackRunner;

@end

@implementation CRUUpdateCheckXPCServiceImpl {
  updater::UpdateService* _service;
  scoped_refptr<updater::AppServerMac> _appServer;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
  NSXPCConnectionOptions _updateCheckXPCConnectionOptions;
  base::scoped_nsobject<NSXPCConnection> _updateCheckXPCConnection;
  NSInteger _redialAttempts;
}

- (instancetype)
    initWithUpdateService:(updater::UpdateService*)service
                appServer:(scoped_refptr<updater::AppServerMac>)appServer
           callbackRunner:
               (scoped_refptr<base::SequencedTaskRunner>)callbackRunner {
  if (self = [super init]) {
    _service = service;
    _appServer = appServer;
    _callbackRunner = callbackRunner;
  }
  return self;
}

- (instancetype)
       initWithUpdateService:(updater::UpdateService*)service
                   appServer:(scoped_refptr<updater::AppServerMac>)appServer
    updaterConnectionOptions:(NSXPCConnectionOptions)options
              callbackRunner:
                  (scoped_refptr<base::SequencedTaskRunner>)callbackRunner {
  [self initWithUpdateService:service
                    appServer:appServer
               callbackRunner:callbackRunner];
  _updateCheckXPCConnectionOptions = options;
  [self dialUpdateCheckXPCConnection];
  return self;
}

- (void)dialUpdateCheckXPCConnection {
  _updateCheckXPCConnection.reset([[NSXPCConnection alloc]
      initWithMachServiceName:updater::GetServiceMachName().get()
                      options:_updateCheckXPCConnectionOptions]);

  _updateCheckXPCConnection.get().remoteObjectInterface =
      updater::GetXPCUpdateCheckingInterface();

  _updateCheckXPCConnection.get().interruptionHandler = ^{
    LOG(WARNING) << "CRUUpdateCheckingService: XPC connection interrupted.";
  };

  _updateCheckXPCConnection.get().invalidationHandler = ^{
    LOG(WARNING) << "CRUUpdateCheckingService: XPC connection invalidated.";
  };
  [_updateCheckXPCConnection resume];
}

#pragma mark CRUUpdateChecking
- (void)checkForUpdatesWithUpdateState:(id<CRUUpdateStateObserving>)updateState
                                 reply:(void (^_Nonnull)(int rc))reply {
  auto cb =
      base::BindOnce(base::RetainBlock(^(updater::UpdateService::Result error) {
        VLOG(0) << "UpdateAll complete: error = " << static_cast<int>(error);
        if (reply)
          reply(static_cast<int>(error));

        _appServer->TaskCompleted();
      }));

  auto sccb = base::BindRepeating(base::RetainBlock(^(
      updater::UpdateService::UpdateState state) {
    NSString* version = base::SysUTF8ToNSString(
        state.next_version.IsValid() ? state.next_version.GetString() : "");

    base::scoped_nsobject<CRUUpdateStateStateWrapper> updateStateStateWrapper(
        [[CRUUpdateStateStateWrapper alloc]
            initWithUpdateStateState:state.state],
        base::scoped_policy::RETAIN);
    base::scoped_nsobject<CRUErrorCategoryWrapper> errorCategoryWrapper(
        [[CRUErrorCategoryWrapper alloc]
            initWithErrorCategory:state.error_category],
        base::scoped_policy::RETAIN);

    base::scoped_nsobject<CRUUpdateStateWrapper> updateStateWrapper(
        [[CRUUpdateStateWrapper alloc]
              initWithAppId:base::SysUTF8ToNSString(state.app_id)
                      state:updateStateStateWrapper.get()
                    version:version
            downloadedBytes:state.downloaded_bytes
                 totalBytes:state.total_bytes
            installProgress:state.install_progress
              errorCategory:errorCategoryWrapper.get()
                  errorCode:state.error_code
                  extraCode:state.extra_code1]);
    [updateState observeUpdateState:updateStateWrapper.get()];
  }));

  _appServer->TaskStarted();
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

        _appServer->TaskCompleted();
      }));

  auto sccb = base::BindRepeating(base::RetainBlock(^(
      updater::UpdateService::UpdateState state) {
    NSString* version = base::SysUTF8ToNSString(
        state.next_version.IsValid() ? state.next_version.GetString() : "");

    base::scoped_nsobject<CRUUpdateStateStateWrapper> updateStateStateWrapper(
        [[CRUUpdateStateStateWrapper alloc]
            initWithUpdateStateState:state.state],
        base::scoped_policy::RETAIN);
    base::scoped_nsobject<CRUErrorCategoryWrapper> errorCategoryWrapper(
        [[CRUErrorCategoryWrapper alloc]
            initWithErrorCategory:state.error_category],
        base::scoped_policy::RETAIN);

    base::scoped_nsobject<CRUUpdateStateWrapper> updateStateWrapper(
        [[CRUUpdateStateWrapper alloc]
              initWithAppId:base::SysUTF8ToNSString(state.app_id)
                      state:updateStateStateWrapper.get()
                    version:version
            downloadedBytes:state.downloaded_bytes
                 totalBytes:state.total_bytes
            installProgress:state.install_progress
              errorCategory:errorCategoryWrapper.get()
                  errorCode:state.error_code
                  extraCode:state.extra_code1]);
    [updateState observeUpdateState:updateStateWrapper.get()];
  }));

  _appServer->TaskStarted();
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

        _appServer->TaskCompleted();
      }));

  _appServer->TaskStarted();
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
  const base::Version selfVersion = GetSelfVersion();
  CHECK(selfVersion.IsValid());
  const base::Version candidateVersion =
      base::Version(base::SysNSStringToUTF8(version));
  if (candidateVersion.IsValid()) {
    return candidateVersion.CompareTo(selfVersion) > 0;
  }
  return NO;
}

- (void)haltForUpdateToVersion:(NSString* _Nonnull)candidateVersion
                         reply:(void (^_Nonnull)(BOOL shouldUpdate))reply {
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

- (void)promoteCandidate {
  // TODO: crbug 1072061
  // Actually do the self test. For now assume it passes.
  BOOL selfTestPassed = YES;

  if (!selfTestPassed) {
    LOG(ERROR) << "Candidate versioned: '"
               << UPDATER_VERSION_STRING "' failed self test.";
    return;
  }

  auto haltErrorHandler = ^(NSError* haltError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([haltError description]);
    // Try to redial the connection
    if (++_redialAttempts <= kMaxRedialAttempts) {
      [self dialUpdateCheckXPCConnection];
      [self promoteCandidate];
      return;
    } else {
      LOG(ERROR) << "XPC connection redialed maximum number of times.";
    }
  };

  auto haltReply = ^(BOOL halted) {
    VLOG(0) << "Response from  haltForUpdateToVersion:reply: = " << halted;
    if (halted) {
      updater::PromoteCandidate();
    } else {
      LOG(ERROR) << "The active service refused to halt for update to version: "
                 << UPDATER_VERSION_STRING;
    }
  };

  [[_updateCheckXPCConnection.get()
      remoteObjectProxyWithErrorHandler:haltErrorHandler]
      haltForUpdateToVersion:@UPDATER_VERSION_STRING
                       reply:haltReply];
}

- (void)performAdminTasks {
  base::scoped_nsobject<NSError> versionError;

  if (!_updateCheckXPCConnection) {
    [self promoteCandidate];
    return;
  }

  auto versionErrorHandler = ^(NSError* versionError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([versionError description]);
  };

  auto versionReply = ^(NSString* _Nullable activeServiceVersionString) {
    if (activeServiceVersionString) {
      const base::Version activeServiceVersion =
          base::Version(base::SysNSStringToUTF8(activeServiceVersionString));
      const base::Version selfVersion = GetSelfVersion();

      if (!selfVersion.IsValid()) {
        updater::UninstallCandidate();
      } else if (!activeServiceVersion.IsValid()) {
        [self promoteCandidate];
      } else {
        int versionComparisonResult =
            selfVersion.CompareTo(activeServiceVersion);

        if (versionComparisonResult > 0) {
          // If the versioned service is a higher version than the active
          // service, run a self test and activate the versioned service.
          [self promoteCandidate];
        } else if (versionComparisonResult < 0) {
          // If the versioned service is a lower version than the active
          // service, remove the versioned service.
          updater::UninstallCandidate();
        } else {
          // If the versioned service is the same version as the active
          // service, check for updates.
          base::scoped_nsobject<NSError> updateCheckError;

          base::scoped_nsprotocol<id<CRUUpdateStateObserving>> stateObserver(
              [[CRUUpdateStateObserver alloc]
                  initWithRepeatingCallback:
                      base::BindRepeating(
                          [](updater::UpdateService::UpdateState) {})
                             callbackRunner:_callbackRunner]);

          auto updateCheckErrorHandler = ^(NSError* updateCheckError) {
            LOG(ERROR) << "XPC connection failed: "
                       << base::SysNSStringToUTF8(
                              [updateCheckError description]);
          };
          auto updateCheckReply = ^(int error) {
            VLOG(0) << "UpdateAll complete: exit_code = " << error;
          };

          [[_updateCheckXPCConnection.get()
              remoteObjectProxyWithErrorHandler:updateCheckErrorHandler]
              checkForUpdatesWithUpdateState:stateObserver
                                       reply:updateCheckReply];
        }
      }
    } else {
      // Active service version is nil.
      LOG(ERROR) << "Active service version is nil.";
    }
  };

  [[_updateCheckXPCConnection.get()
      remoteObjectProxyWithErrorHandler:versionErrorHandler]
      getUpdaterVersionWithReply:versionReply];
}

@end

@implementation CRUUpdateCheckXPCServiceDelegate {
  scoped_refptr<updater::UpdateService> _service;
  scoped_refptr<updater::AppServerMac> _appServer;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)
    initWithUpdateService:(scoped_refptr<updater::UpdateService>)service
                appServer:(scoped_refptr<updater::AppServerMac>)appServer {
  if (self = [super init]) {
    _service = service;
    _appServer = appServer;
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
                      appServer:_appServer
                 callbackRunner:_callbackRunner.get()]);
  newConnection.exportedObject = object.get();
  [newConnection resume];
  return YES;
}

@end

@implementation CRUAdministrationXPCServiceDelegate {
  scoped_refptr<updater::UpdateService> _service;
  scoped_refptr<updater::AppServerMac> _appServer;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

- (instancetype)
    initWithUpdateService:(scoped_refptr<updater::UpdateService>)service
                appServer:(scoped_refptr<updater::AppServerMac>)appServer {
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

  base::CommandLine* cmdLine = base::CommandLine::ForCurrentProcess();
  NSXPCConnectionOptions options = cmdLine->HasSwitch(updater::kSystemSwitch)
                                       ? NSXPCConnectionPrivileged
                                       : 0;

  newConnection.exportedInterface = updater::GetXPCAdministeringInterface();

  base::scoped_nsobject<CRUUpdateCheckXPCServiceImpl> object(
      [[CRUUpdateCheckXPCServiceImpl alloc]
             initWithUpdateService:_service.get()
                         appServer:_appServer
          updaterConnectionOptions:options
                    callbackRunner:_callbackRunner.get()]);
  newConnection.exportedObject = object.get();
  [newConnection resume];
  return YES;
}

@end
