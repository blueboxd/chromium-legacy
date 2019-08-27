// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_impl.h"

#include "content/public/browser/navigation_handle.h"

namespace weblayer {

NavigationImpl::NavigationImpl(content::NavigationHandle* navigation_handle)
    : navigation_handle_(navigation_handle) {}

NavigationImpl::~NavigationImpl() = default;

GURL NavigationImpl::GetURL() {
  return navigation_handle_->GetURL();
}

const std::vector<GURL>& NavigationImpl::GetRedirectChain() {
  return navigation_handle_->GetRedirectChain();
}

Navigation::State NavigationImpl::GetState() {
  NOTIMPLEMENTED() << "TODO: properly implement this";
  return Navigation::State::kWaitingResponse;
}

}  // namespace weblayer
