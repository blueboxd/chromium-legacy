// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_IOS_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_IOS_H_

#import <UIKit/UIKit.h>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_controller_delegate.h"

@protocol InfobarUIDelegate;
namespace infobars {
class InfoBarDelegate;
}

// The iOS version of infobars::InfoBar.
class InfoBarIOS : public infobars::InfoBar, public InfoBarControllerDelegate {
 public:
  InfoBarIOS(id<InfobarUIDelegate> controller,
             std::unique_ptr<infobars::InfoBarDelegate> delegate,
             bool skip_banner = false);
  ~InfoBarIOS() override;
  InfoBarIOS(const InfoBarIOS&) = delete;
  InfoBarIOS& operator=(const InfoBarIOS&) = delete;

  // Observer interface for objects interested in changes to InfoBarIOS.
  class Observer : public base::CheckedObserver {
   public:
    // Called when |infobar|'s accepted() is set to a new value.
    virtual void DidUpdateAcceptedState(InfoBarIOS* infobar) {}

    // Called when |infobar| is destroyed.
    virtual void InfobarDestroyed(InfoBarIOS* infobar) {}
  };

  // Adds and removes observers.
  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }
  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

  // Whether or not the infobar has been accepted.  Set to true when the
  // associated action has been executed (e.g. page translation finished), and
  // false if the action has not been executed or has been reverted.
  bool accepted() const { return accepted_; }
  void set_accepted(bool accepted);

  // Whether or not the banner should be skipped. If true if the banner
  // should be skipped but not the badge and subsequent modals.
  bool skip_banner() const { return skip_banner_; }

  // Returns the InfobarUIDelegate associated to this Infobar.
  id<InfobarUIDelegate> InfobarUIDelegate();

  // Remove the infobar view from infobar container view.
  void RemoveView();

  // Returns a weak pointer to the infobar.
  base::WeakPtr<InfoBarIOS> GetWeakPtr();

 private:
  // InfoBarControllerDelegate overrides:
  bool IsOwned() override;
  void RemoveInfoBar() override;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
  id<InfobarUIDelegate> controller_ = nil;
  bool accepted_ = false;
  bool skip_banner_ = false;
  base::WeakPtrFactory<InfoBarIOS> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_IOS_H_
