// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/uninstall_dialog.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/public/cpp/icon_loader.h"

namespace apps {

UninstallDialog::UninstallDialog(Profile* profile,
                                 apps::mojom::AppType app_type,
                                 const std::string& app_id,
                                 apps::mojom::IconKeyPtr icon_key,
                                 apps::IconLoader* icon_loader,
                                 UninstallCallback uninstall_callback)
    : profile_(profile),
      app_type_(app_type),
      app_id_(app_id),
      uninstall_callback_(std::move(uninstall_callback)) {
  constexpr bool kAllowPlaceholderIcon = false;
  icon_loader->LoadIconFromIconKey(
      app_type, app_id, std::move(icon_key),
      apps::mojom::IconCompression::kUncompressed, kSizeHintInDip,
      kAllowPlaceholderIcon,
      base::BindOnce(&UninstallDialog::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr()));
}

UninstallDialog::~UninstallDialog() = default;

void UninstallDialog::OnDialogClosed(bool uninstall,
                                     bool clear_site_data,
                                     bool report_abuse) {
  std::move(uninstall_callback_)
      .Run(uninstall, clear_site_data, report_abuse, this);
}

void UninstallDialog::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    return;
  }

  UiBase::Create(profile_, app_type_, app_id_, icon_value->uncompressed, this);
}

}  // namespace apps
