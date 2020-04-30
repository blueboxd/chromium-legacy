// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_notifier_impl.h"

AppListNotifierImpl::AppListNotifierImpl() = default;
AppListNotifierImpl::~AppListNotifierImpl() = default;

void AppListNotifierImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppListNotifierImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppListNotifierImpl::NotifyLaunch(ash::SearchResultDisplayType location,
                                       const std::string& result) {
  // TODO(crbug.com/1076270): Add logic to convert into impression, launch, and
  // abandon events.
}

void AppListNotifierImpl::NotifyResultsUpdated(
    ash::SearchResultDisplayType location,
    const std::vector<std::string>& results) {
  // TODO(crbug.com/1076270): Add logic to convert into impression, launch, and
  // abandon events.
}

void AppListNotifierImpl::NotifySearchQueryChanged(
    const base::string16& query) {
  // TODO(crbug.com/1076270): Add logic to convert into impression, launch, and
  // abandon events.
}

void AppListNotifierImpl::NotifyUIStateChanged(ash::AppListViewState state) {
  // TODO(crbug.com/1076270): Add logic to convert into impression, launch, and
  // abandon events.
}
