// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"

#include "base/strings/stringprintf.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"
#include "ios/web/public/browser_state.h"

void BreadcrumbManagerKeyedService::AddEvent(const std::string& event) {
  std::string event_log =
      base::StringPrintf("%s%s", browsing_mode_.c_str(), event.c_str());
  breadcrumb_manager_->AddEvent(event_log);
}

void BreadcrumbManagerKeyedService::AddObserver(
    BreadcrumbManagerObserver* observer) {
  breadcrumb_manager_->AddObserver(observer);
}

void BreadcrumbManagerKeyedService::RemoveObserver(
    BreadcrumbManagerObserver* observer) {
  breadcrumb_manager_->RemoveObserver(observer);
}

const std::list<std::string> BreadcrumbManagerKeyedService::GetEvents(
    size_t event_count_limit) const {
  return breadcrumb_manager_->GetEvents(event_count_limit);
}

BreadcrumbManagerKeyedService::BreadcrumbManagerKeyedService(
    web::BrowserState* browser_state)
    // Set "I" for Incognito (Chrome branded OffTheRecord implementation) and
    // empty string for Normal browsing mode.
    : browsing_mode_(browser_state->IsOffTheRecord() ? "I " : ""),
      breadcrumb_manager_(std::make_unique<BreadcrumbManager>()) {}

BreadcrumbManagerKeyedService::~BreadcrumbManagerKeyedService() = default;
