// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_state.h"

#import "base/ios/crb_protocol_observers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SceneStateObserverList : CRBProtocolObservers <SceneStateObserver>
@end

@implementation SceneStateObserverList
@end

#pragma mark - SceneState

@interface SceneState ()

// Container for this object's observers.
@property(nonatomic, strong) SceneStateObserverList* observers;

@end

@implementation SceneState

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [SceneStateObserverList
        observersWithProtocol:@protocol(SceneStateObserver)];
  }
  return self;
}

#pragma mark - public

- (void)addObserver:(id<SceneStateObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<SceneStateObserver>)observer {
  [self.observers removeObserver:observer];
}

#pragma mark - Setters & Getters.

- (void)setActivationLevel:(SceneActivationLevel)newLevel {
  if (_activationLevel == newLevel) {
    return;
  }
  _activationLevel = newLevel;

  [self.observers sceneState:self transitionedToActivationLevel:newLevel];
}

#pragma mark - debug

- (NSString*)description {
  NSString* activityString = nil;
  switch (self.activationLevel) {
    case SceneActivationLevelUnattached: {
      activityString = @"Unattached";
      break;
    }

    case SceneActivationLevelBackground: {
      activityString = @"Background";
      break;
    }
    case SceneActivationLevelForegroundInactive: {
      activityString = @"Foreground, Inactive";
      break;
    }
    case SceneActivationLevelForegroundActive: {
      activityString = @"Active";
      break;
    }
  }

  return
      [NSString stringWithFormat:@"SceneState %p (%@)", self, activityString];
}

@end
