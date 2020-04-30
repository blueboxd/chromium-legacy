// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_NOTIFIER_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_NOTIFIER_IMPL_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"

class AppListNotifierImpl : public ash::AppListNotifier {
 public:
  AppListNotifierImpl();
  ~AppListNotifierImpl() override;

  AppListNotifierImpl(const AppListNotifierImpl&) = delete;
  AppListNotifierImpl& operator=(const AppListNotifierImpl&) = delete;

  // AppListNotifier:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyLaunch(ash::SearchResultDisplayType location,
                    const std::string& result) override;
  void NotifyResultsUpdated(ash::SearchResultDisplayType location,
                            const std::vector<std::string>& results) override;
  void NotifySearchQueryChanged(const base::string16& query) override;
  void NotifyUIStateChanged(ash::AppListViewState state) override;

 private:
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<AppListNotifierImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_NOTIFIER_IMPL_H_
