// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/update_service_out_of_process.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#import "chrome/updater/server/mac/service_protocol.h"
#include "chrome/updater/updater_version.h"
#include "components/update_client/update_client_errors.h"

// Interface to communicate with the XPC Updater Service.
@interface CRUUpdateServiceOutOfProcessImpl : NSObject <CRUUpdateChecking>

@end

@implementation CRUUpdateServiceOutOfProcessImpl {
  base::scoped_nsobject<NSXPCConnection> _xpcConnection;
}

- (instancetype)init {
  if ((self = [super init])) {
    std::string serviceName = MAC_BUNDLE_IDENTIFIER_STRING;
    serviceName.append(".UpdaterXPCService");
    _xpcConnection.reset([[NSXPCConnection alloc]
        initWithServiceName:base::SysUTF8ToNSString(serviceName)]);
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

@end

namespace updater {

UpdateServiceOutOfProcess::UpdateServiceOutOfProcess() {
  _client.reset([[CRUUpdateServiceOutOfProcessImpl alloc] init]);
}

void UpdateServiceOutOfProcess::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
  // TODO(crbug.com/1055942): Implement.
}

void UpdateServiceOutOfProcess::UpdateAll(
    base::OnceCallback<void(update_client::Error)> callback) {
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

UpdateServiceOutOfProcess::~UpdateServiceOutOfProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater
