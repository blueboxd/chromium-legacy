// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/update_service_out_of_process.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "chrome/updater/mac/xpc_service_names.h"
#import "chrome/updater/server/mac/service_protocol.h"
#import "chrome/updater/server/mac/update_service_wrappers.h"
#include "chrome/updater/updater_version.h"
#include "components/update_client/update_client_errors.h"

using base::SysUTF8ToNSString;

// Interface to communicate with the XPC Updater Service.
@interface CRUUpdateServiceOutOfProcessImpl : NSObject <CRUUpdateChecking>

@end

@implementation CRUUpdateServiceOutOfProcessImpl {
  base::scoped_nsobject<NSXPCConnection> _xpcConnection;
}

- (instancetype)init {
  if ((self = [super init])) {
    _xpcConnection.reset([[NSXPCConnection alloc]
        initWithServiceName:updater::GetGoogleUpdateServiceMachName().get()]);
    _xpcConnection.get().remoteObjectInterface =
        [NSXPCInterface interfaceWithProtocol:@protocol(CRUUpdateChecking)];

    [_xpcConnection resume];
  }

  return self;
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                                tag:(NSString* _Nullable)tag
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nullable)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: " << [xpcError description];
    reply(-1);
  };

  [[_xpcConnection.get() remoteObjectProxyWithErrorHandler:errorHandler]
      registerForUpdatesWithAppId:appId
                        brandCode:brandCode
                              tag:tag
                          version:version
             existenceCheckerPath:existenceCheckerPath
                            reply:reply];
}

- (void)checkForUpdatesWithReply:(void (^_Nullable)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: " << [xpcError description];
    reply(-1);
  };

  [[_xpcConnection.get() remoteObjectProxyWithErrorHandler:errorHandler]
      checkForUpdatesWithReply:reply];
}

- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
                       priority:(CRUPriorityWrapper* _Nonnull)priority
                    updateState:(CRUUpdateStateObserver* _Nonnull)updateState
                          reply:(void (^_Nullable)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: " << [xpcError description];
    reply(-1);
  };

  [[_xpcConnection.get() remoteObjectProxyWithErrorHandler:errorHandler]
      checkForUpdateWithAppID:appID
                     priority:priority
                  updateState:updateState
                        reply:reply];
}

@end

namespace updater {

UpdateServiceOutOfProcess::UpdateServiceOutOfProcess() {
  _client.reset([[CRUUpdateServiceOutOfProcessImpl alloc] init]);
}

void UpdateServiceOutOfProcess::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  __block base::OnceCallback<void(const RegistrationResponse&)> block_callback =
      std::move(callback);

  auto reply = ^(int error) {
    RegistrationResponse response;
    response.status_code = error;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback), response));
  };

  [_client.get()
      registerForUpdatesWithAppId:SysUTF8ToNSString(request.app_id)
                        brandCode:SysUTF8ToNSString(request.brand_code)
                              tag:SysUTF8ToNSString(request.tag)
                          version:SysUTF8ToNSString(request.version.GetString())
             existenceCheckerPath:SysUTF8ToNSString(
                                      request.existence_checker_path
                                          .AsUTF8Unsafe())
                            reply:reply];
}

void UpdateServiceOutOfProcess::UpdateAll(
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  __block base::OnceCallback<void(update_client::Error)> block_callback =
      std::move(callback);
  auto reply = ^(int error) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback),
                                  static_cast<update_client::Error>(error)));
  };

  [_client.get() checkForUpdatesWithReply:reply];
}

void UpdateServiceOutOfProcess::Update(const std::string& app_id,
                                       UpdateService::Priority priority,
                                       StateChangeCallback state_update,
                                       base::OnceCallback<void(Result)> done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  __block base::OnceCallback<void(update_client::Error)> block_callback =
      std::move(done);
  auto reply = ^(int error) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback),
                                  static_cast<update_client::Error>(error)));
  };

  base::scoped_nsobject<CRUPriorityWrapper> priorityWrapper(
      [[CRUPriorityWrapper alloc] initWithPriority:priority]);
  base::scoped_nsobject<CRUUpdateStateObserver> stateObserver(
      [[CRUUpdateStateObserver alloc] initWithRepeatingCallback:state_update]);

  [_client.get() checkForUpdateWithAppID:SysUTF8ToNSString(app_id)
                                priority:priorityWrapper.get()
                             updateState:stateObserver.get()
                                   reply:reply];
}

UpdateServiceOutOfProcess::~UpdateServiceOutOfProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater
