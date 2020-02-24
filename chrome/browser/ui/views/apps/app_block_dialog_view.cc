// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_block_dialog_view.h"

#include "chrome/browser/apps/app_service/arc_apps.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

AppBlockDialogView* g_app_block_dialog_view = nullptr;

}  // namespace

namespace apps {

// static
void ArcApps::CreateBlockDialog(const std::string& app_name,
                                const gfx::ImageSkia& image,
                                Profile* profile) {
  views::DialogDelegate::CreateDialogWidget(
      new AppBlockDialogView(app_name, image, profile), nullptr, nullptr)
      ->Show();
}

}  // namespace apps

// static
AppBlockDialogView* AppBlockDialogView::GetActiveViewForTesting() {
  return g_app_block_dialog_view;
}

AppBlockDialogView::AppBlockDialogView(const std::string& app_name,
                                       const gfx::ImageSkia& image,
                                       Profile* profile) {
  base::string16 heading_text = l10n_util::GetStringFUTF16(
      profile->IsChild() ? IDS_APP_BLOCK_HEADING_FOR_CHILD
                         : IDS_APP_BLOCK_HEADING,
      base::UTF8ToUTF16(app_name));

  InitializeView(image, heading_text);

  g_app_block_dialog_view = this;
}

AppBlockDialogView::~AppBlockDialogView() {
  g_app_block_dialog_view = nullptr;
}

base::string16 AppBlockDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_APP_BLOCK_PROMPT_TITLE);
}
