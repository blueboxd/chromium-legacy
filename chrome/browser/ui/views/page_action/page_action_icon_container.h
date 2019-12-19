// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTAINER_H_

#include "ui/views/view.h"

class PageActionIconController;
struct PageActionIconParams;

// Class implemented by a container view that holds page action icons.
class PageActionIconContainer {
 public:
  // Adds a page action icon to the container view. The container can
  // determine where to place and how to lay out the icons.
  virtual void AddPageActionIcon(views::View* icon) = 0;
};

// Implements a default icon container for page action icons.
class PageActionIconContainerView : public views::View,
                                    public PageActionIconContainer {
 public:
  explicit PageActionIconContainerView(const PageActionIconParams& params);
  ~PageActionIconContainerView() override;

  PageActionIconController* controller() { return controller_.get(); }

  // views::View:
  const char* GetClassName() const override;

  static const char kPageActionIconContainerViewClassName[];

 private:
  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // PageActionIconContainer:
  void AddPageActionIcon(views::View* icon) override;

  std::unique_ptr<PageActionIconController> controller_;

  DISALLOW_COPY_AND_ASSIGN(PageActionIconContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTAINER_H_
