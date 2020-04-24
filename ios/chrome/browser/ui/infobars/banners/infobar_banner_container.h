// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_CONTAINER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_CONTAINER_H_

// Protocol for the InfobarBanners to communicate with the InfobarContainer
// Coordinator if needed.
@protocol InfobarBannerContainer

// Called whener an InfobarBannerContained banner has been dismissed.
- (void)infobarBannerFinishedPresenting;

// YES if the InfobarBannerContainer shouldn't be presenting any InfobarBanners,
// meaning that the InfobarBanner needs to be dismissed.
- (BOOL)shouldDismissBanner;

@end

// Infobar banners presented by an InfobarCoordinatorContainer.
@protocol InfobarBannerContained

// Container managing InfobarBanner presentations.
@property(nonatomic, weak) id<InfobarBannerContainer> infobarBannerContainer;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_CONTAINER_H_
