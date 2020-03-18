// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/server/mac/update_service_wrappers.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#import "chrome/updater/server/mac/service_protocol.h"

static NSString* const kCRUUpdateState = @"updateState";
static NSString* const kCRUPriority = @"priority";

using StateChangeCallback =
    base::RepeatingCallback<void(updater::UpdateService::UpdateState)>;

@implementation CRUUpdateStateObserver {
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

@synthesize callback = _callback;

- (instancetype)initWithRepeatingCallback:(StateChangeCallback)callback
                           callbackRunner:
                               (scoped_refptr<base::SequencedTaskRunner>)
                                   callbackRunner {
  if (self = [super init]) {
    _callback = callback;
    _callbackRunner = callbackRunner;
  }
  return self;
}

- (void)observeUpdateState:(CRUUpdateStateWrapper* _Nonnull)updateState {
  _callbackRunner->PostTask(
      FROM_HERE, base::BindRepeating(_callback, [updateState updateState]));
}

@end

@implementation CRUUpdateStateWrapper

@synthesize updateState = _updateState;

// Wrapper for updater::UpdateService::UpdateState
typedef NS_ENUM(NSInteger, CRUUpdateStateEnum) {
  kCRUUpdateStateUnknown =
      static_cast<NSInteger>(updater::UpdateService::UpdateState::kUnknown),
  kCRUUpdateStateNotStarted =
      static_cast<NSInteger>(updater::UpdateService::UpdateState::kNotStarted),
  kCRUUpdateStateCheckingForUpdates = static_cast<NSInteger>(
      updater::UpdateService::UpdateState::kCheckingForUpdates),
  kCRUUpdateStateDownloading =
      static_cast<NSInteger>(updater::UpdateService::UpdateState::kDownloading),
  kCRUUpdateStateInstalling =
      static_cast<NSInteger>(updater::UpdateService::UpdateState::kInstalling),
  kCRUUpdateStateUpdated =
      static_cast<NSInteger>(updater::UpdateService::UpdateState::kUpdated),
  kCRUUpdateStateNoUpdate =
      static_cast<NSInteger>(updater::UpdateService::UpdateState::kNoUpdate),
  kCRUUpdateStateUpdateError =
      static_cast<NSInteger>(updater::UpdateService::UpdateState::kUpdateError),
};

// Designated initializer.
- (instancetype)initWithUpdateState:
    (updater::UpdateService::UpdateState)updateState {
  if (self = [super init]) {
    _updateState = updateState;
  }
  return self;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  DCHECK([aDecoder allowsKeyedCoding]);
  NSInteger enumValue = [aDecoder decodeIntegerForKey:kCRUUpdateState];

  switch (enumValue) {
    case kCRUUpdateStateUnknown:
      return [self
          initWithUpdateState:updater::UpdateService::UpdateState::kUnknown];
    case kCRUUpdateStateNotStarted:
      return [self
          initWithUpdateState:updater::UpdateService::UpdateState::kNotStarted];
    case kCRUUpdateStateCheckingForUpdates:
      return [self initWithUpdateState:updater::UpdateService::UpdateState::
                                           kCheckingForUpdates];
    case kCRUUpdateStateDownloading:
      return [self initWithUpdateState:updater::UpdateService::UpdateState::
                                           kDownloading];
    case kCRUUpdateStateInstalling:
      return [self
          initWithUpdateState:updater::UpdateService::UpdateState::kInstalling];
    case kCRUUpdateStateUpdated:
      return [self
          initWithUpdateState:updater::UpdateService::UpdateState::kUpdated];
    case kCRUUpdateStateNoUpdate:
      return [self
          initWithUpdateState:updater::UpdateService::UpdateState::kNoUpdate];
    case kCRUUpdateStateUpdateError:
      return [self initWithUpdateState:updater::UpdateService::UpdateState::
                                           kUpdateError];
    default:
      DLOG(ERROR) << "Unexpected value for CRUUpdateStateEnum: " << enumValue;
      return nil;
  }
}

- (void)encodeWithCoder:(NSCoder*)coder {
  DCHECK([coder respondsToSelector:@selector(encodeInt:forKey:)]);
  [coder encodeInt:static_cast<NSInteger>(_updateState) forKey:kCRUUpdateState];
}

@end

@implementation CRUPriorityWrapper

@synthesize priority = _priority;

// Wrapper for updater::UpdateService::Priority
typedef NS_ENUM(NSInteger, CRUUpdatePriorityEnum) {
  kCRUUpdatePriorityUnknown =
      static_cast<NSInteger>(updater::UpdateService::Priority::kUnknown),
  kCRUUpdatePriorityBackground =
      static_cast<NSInteger>(updater::UpdateService::Priority::kBackground),
  kCRUUpdatePriorityForeground =
      static_cast<NSInteger>(updater::UpdateService::Priority::kForeground),
};

// Designated initializer.
- (instancetype)initWithPriority:(updater::UpdateService::Priority)priority {
  if (self = [super init]) {
    _priority = priority;
  }
  return self;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  DCHECK([aDecoder allowsKeyedCoding]);
  NSInteger enumValue = [aDecoder decodeIntegerForKey:kCRUPriority];

  switch (enumValue) {
    case kCRUUpdatePriorityUnknown:
      return [self initWithPriority:updater::UpdateService::Priority::kUnknown];
    case kCRUUpdatePriorityBackground:
      return
          [self initWithPriority:updater::UpdateService::Priority::kBackground];
    case kCRUUpdatePriorityForeground:
      return
          [self initWithPriority:updater::UpdateService::Priority::kForeground];
    default:
      DLOG(ERROR) << "Unexpected value for CRUUpdatePriorityEnum: "
                  << enumValue;
      return nil;
  }
}
- (void)encodeWithCoder:(NSCoder*)coder {
  DCHECK([coder respondsToSelector:@selector(encodeInt:forKey:)]);
  [coder encodeInt:static_cast<NSInteger>(_priority) forKey:kCRUPriority];
}

@end
