// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_CONTEXT_MENU_OBSERVER_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_CONTEXT_MENU_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

namespace gfx {
class ImageSkia;
}

class RenderViewContextMenuProxy;

class SharingService;

class ClickToCallContextMenuObserver
    : public RenderViewContextMenuObserver,
      public base::SupportsWeakPtr<ClickToCallContextMenuObserver> {
 public:
  class SubMenuDelegate : public ui::SimpleMenuModel::Delegate {
   public:
    explicit SubMenuDelegate(ClickToCallContextMenuObserver* parent);
    ~SubMenuDelegate() override;

    bool IsCommandIdEnabled(int command_id) const override;
    void ExecuteCommand(int command_id, int event_flags) override;

   private:
    ClickToCallContextMenuObserver* const parent_;

    DISALLOW_COPY_AND_ASSIGN(SubMenuDelegate);
  };

  explicit ClickToCallContextMenuObserver(RenderViewContextMenuProxy* proxy);
  ~ClickToCallContextMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ClickToCallContextMenuObserverTest,
                           SingleDevice_ShowMenu);
  FRIEND_TEST_ALL_PREFIXES(ClickToCallContextMenuObserverTest,
                           MultipleDevices_ShowMenu);
  FRIEND_TEST_ALL_PREFIXES(ClickToCallContextMenuObserverTest,
                           MultipleDevices_MoreThanMax_ShowMenu);

  void BuildSubMenu();

  void SendClickToCallMessage(int chosen_device_index);

  gfx::ImageSkia GetContextMenuIcon() const;

  RenderViewContextMenuProxy* proxy_ = nullptr;

  SharingService* sharing_service_ = nullptr;

  SubMenuDelegate sub_menu_delegate_{this};

  GURL url_;

  std::vector<SharingDeviceInfo> devices_;

  std::unique_ptr<ui::SimpleMenuModel> sub_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(ClickToCallContextMenuObserver);
};

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_CONTEXT_MENU_OBSERVER_H_
