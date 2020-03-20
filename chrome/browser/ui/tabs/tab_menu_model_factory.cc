// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"

#include "chrome/browser/ui/tabs/tab_menu_model.h"

std::unique_ptr<ui::SimpleMenuModel> TabMenuModelFactory::Create(
    ui::SimpleMenuModel::Delegate* delegate,
    TabStripModel* tab_strip,
    int index) {
  return std::make_unique<TabMenuModel>(delegate, tab_strip, index);
}
