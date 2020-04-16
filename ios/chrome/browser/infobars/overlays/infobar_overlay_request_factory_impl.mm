// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_factory_impl.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

InfobarOverlayRequestFactoryImpl::InfobarOverlayRequestFactoryImpl() {
  // Create the factory helpers for the supported infobar types.
  // TODO(crbug.com/1030357): Add factory helpers for other infobar and overlay
  // types.
  SetUpFactories(InfobarType::kInfobarTypePasswordSave,
                 CreateFactory<SavePasswordInfobarBannerOverlayRequestConfig>(),
                 /*detail_sheet_factory=*/nullptr,
                 CreateFactory<PasswordInfobarModalOverlayRequestConfig>());
}

InfobarOverlayRequestFactoryImpl::~InfobarOverlayRequestFactoryImpl() = default;

std::unique_ptr<OverlayRequest>
InfobarOverlayRequestFactoryImpl::CreateInfobarRequest(
    InfoBar* infobar,
    InfobarOverlayType type) {
  DCHECK(infobar);
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  // TODO(crbug.com/1030357): This factory should DCHECK that |factory| is
  // non-null after all existing infobars have been converted to using overlays.
  // Early return in the interim to prevent crashing while the remaining
  // infobars are being converted.
  FactoryHelper* factory = factory_storages_[infobar_ios->infobar_type()][type];
  return factory ? factory->CreateInfobarRequest(infobar_ios) : nullptr;
}

void InfobarOverlayRequestFactoryImpl::SetUpFactories(
    InfobarType type,
    std::unique_ptr<FactoryHelper> banner_factory,
    std::unique_ptr<FactoryHelper> detail_sheet_factory,
    std::unique_ptr<FactoryHelper> modal_factory) {
  factory_storages_.emplace(
      type, FactoryHelperStorage(std::move(banner_factory),
                                 std::move(detail_sheet_factory),
                                 std::move(modal_factory)));
}

#pragma mark - FactoryHelperStorage

InfobarOverlayRequestFactoryImpl::FactoryHelperStorage::FactoryHelperStorage() =
    default;

InfobarOverlayRequestFactoryImpl::FactoryHelperStorage::FactoryHelperStorage(
    std::unique_ptr<FactoryHelper> banner_factory,
    std::unique_ptr<FactoryHelper> detail_sheet_factory,
    std::unique_ptr<FactoryHelper> modal_factory) {
  factories_[InfobarOverlayType::kBanner] = std::move(banner_factory);
  factories_[InfobarOverlayType::kDetailSheet] =
      std::move(detail_sheet_factory);
  factories_[InfobarOverlayType::kModal] = std::move(modal_factory);
}

InfobarOverlayRequestFactoryImpl::FactoryHelperStorage::FactoryHelperStorage(
    InfobarOverlayRequestFactoryImpl::FactoryHelperStorage&& storage) {
  factories_[InfobarOverlayType::kBanner] =
      std::move(storage.factories_[InfobarOverlayType::kBanner]);
  factories_[InfobarOverlayType::kDetailSheet] =
      std::move(storage.factories_[InfobarOverlayType::kDetailSheet]);
  factories_[InfobarOverlayType::kModal] =
      std::move(storage.factories_[InfobarOverlayType::kModal]);
}

InfobarOverlayRequestFactoryImpl::FactoryHelperStorage::
    ~FactoryHelperStorage() = default;

InfobarOverlayRequestFactoryImpl::FactoryHelper*
    InfobarOverlayRequestFactoryImpl::FactoryHelperStorage::operator[](
        InfobarOverlayType type) {
  return factories_[type].get();
}
