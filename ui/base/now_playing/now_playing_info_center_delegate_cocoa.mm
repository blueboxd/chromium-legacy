// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/now_playing/now_playing_info_center_delegate_cocoa.h"

#import <MediaPlayer/MediaPlayer.h>

#include "base/mac/scoped_nsobject.h"
#include "build/branding_buildflags.h"

@interface NowPlayingInfoCenterDelegateCocoa ()

// Initialize the |nowPlayingInfo_| dictionary with values.
- (void)initializeNowPlayingInfoValues;

// Give MPNowPlayingInfoCenter the updated nowPlayingInfo_ dictionary.
- (void)updateNowPlayingInfo;

@end

@implementation NowPlayingInfoCenterDelegateCocoa {
  base::scoped_nsobject<NSMutableDictionary> nowPlayingInfo_;
}

- (instancetype)init {
  if (self = [super init]) {
    nowPlayingInfo_.reset([[NSMutableDictionary alloc] init]);
    [self resetNowPlayingInfo];
    [self updateNowPlayingInfo];
  }
  return self;
}

- (void)resetNowPlayingInfo {
  [nowPlayingInfo_ removeAllObjects];
  [self initializeNowPlayingInfoValues];
  [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
}

- (void)setPlaybackState:(MPNowPlayingPlaybackState)state {
  [MPNowPlayingInfoCenter defaultCenter].playbackState = state;
  [self updateNowPlayingInfo];
}

- (void)initializeNowPlayingInfoValues {
  [nowPlayingInfo_ setObject:[NSNumber numberWithDouble:0]
                      forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
  [nowPlayingInfo_ setObject:[NSNumber numberWithDouble:0]
                      forKey:MPNowPlayingInfoPropertyPlaybackRate];
  [nowPlayingInfo_ setObject:[NSNumber numberWithDouble:0]
                      forKey:MPMediaItemPropertyPlaybackDuration];
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  [nowPlayingInfo_ setObject:@"Chrome" forKey:MPMediaItemPropertyTitle];
#else
  [nowPlayingInfo_ setObject:@"Chromium" forKey:MPMediaItemPropertyTitle];
#endif
  [nowPlayingInfo_ setObject:@"" forKey:MPMediaItemPropertyArtist];
}

- (void)updateNowPlayingInfo {
  [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nowPlayingInfo_;
}

@end
