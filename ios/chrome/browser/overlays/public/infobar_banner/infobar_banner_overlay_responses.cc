// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"

#pragma mark - InfobarBannerMainActionResponse

OVERLAY_USER_DATA_SETUP_IMPL(InfobarBannerMainActionResponse);

InfobarBannerMainActionResponse::InfobarBannerMainActionResponse() = default;

InfobarBannerMainActionResponse::~InfobarBannerMainActionResponse() = default;

#pragma mark - InfobarBannerShowModalResponse

OVERLAY_USER_DATA_SETUP_IMPL(InfobarBannerShowModalResponse);

InfobarBannerShowModalResponse::InfobarBannerShowModalResponse() = default;

InfobarBannerShowModalResponse::~InfobarBannerShowModalResponse() = default;

#pragma mark - InfobarBannerCompletionResponse

OVERLAY_USER_DATA_SETUP_IMPL(InfobarBannerCompletionResponse);

InfobarBannerCompletionResponse::InfobarBannerCompletionResponse(
    bool user_initiated)
    : user_initiated_(user_initiated) {}

InfobarBannerCompletionResponse::~InfobarBannerCompletionResponse() = default;
