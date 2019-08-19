// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/app_management/app_management_page_handler_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

AppManagementPageHandlerFactory::AppManagementPageHandlerFactory(
    Profile* profile)
    : page_factory_binding_(this), profile_(profile) {}

AppManagementPageHandlerFactory::~AppManagementPageHandlerFactory() = default;

void AppManagementPageHandlerFactory::Bind(
    app_management::mojom::PageHandlerFactoryRequest request) {
  if (page_factory_binding_.is_bound()) {
    page_factory_binding_.Unbind();
  }

  page_factory_binding_.Bind(std::move(request));
}

void AppManagementPageHandlerFactory::BindPageHandlerFactory(
    app_management::mojom::PageHandlerFactoryRequest request) {
  if (page_factory_binding_.is_bound()) {
    page_factory_binding_.Unbind();
  }

  page_factory_binding_.Bind(std::move(request));
}

void AppManagementPageHandlerFactory::CreatePageHandler(
    app_management::mojom::PagePtr page,
    app_management::mojom::PageHandlerRequest request) {
  DCHECK(page);

  page_handler_ = std::make_unique<AppManagementPageHandler>(
      std::move(request), std::move(page), profile_);
}
