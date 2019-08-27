// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_

#include <map>

#include "base/macros.h"
#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "weblayer/browser/navigation_impl.h"
#include "weblayer/public/navigation_controller.h"

namespace weblayer {
class BrowserControllerImpl;

class NavigationControllerImpl : public NavigationController,
                                 public content::WebContentsObserver {
 public:
  explicit NavigationControllerImpl(BrowserControllerImpl* browser_controller);
  ~NavigationControllerImpl() override;

 private:
  // NavigationController implementation:
  void AddObserver(NavigationObserver* observer) override;
  void RemoveObserver(NavigationObserver* observer) override;
  void Navigate(const GURL& url) override;
  void GoBack() override;
  void GoForward() override;
  void Reload() override;
  void Stop() override;
  int GetNavigationListSize() override;
  int GetNavigationListCurrentIndex() override;
  GURL GetNavigationEntryDisplayURL(int index) override;

  // content::WebContentsObserver implementation:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  BrowserControllerImpl* browser_controller_;
  base::ObserverList<NavigationObserver>::Unchecked observers_;
  std::map<content::NavigationHandle*, std::unique_ptr<NavigationImpl>>
      navigation_map_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
