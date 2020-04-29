// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/translate_infobar_placeholder_overlay_request_cancel_handler.h"

#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_infobar_delegate.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/fake_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#include "ios/chrome/browser/infobars/overlays/overlay_request_infobar_util.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/common/placeholder_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/translate_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using translate_infobar_overlays::TranslateBannerRequestConfig;
using translate_infobar_overlays::PlaceholderRequestCancelHandler;

// Fake of TranslateInfoBarDelegate that allows for triggering Observer
// callbacks.
class FakeTranslateInfobarDelegate
    : public translate::TranslateInfoBarDelegate {
 public:
  FakeTranslateInfobarDelegate(
      const base::WeakPtr<translate::TranslateManager>& translate_manager,
      bool is_off_the_record,
      translate::TranslateStep step,
      const std::string& original_language,
      const std::string& target_language,
      translate::TranslateErrors::Type error_type,
      bool triggered_from_menu);
  ~FakeTranslateInfobarDelegate() override = default;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void TriggerOnTranslateStepChanged(
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type);

 private:
  base::ObserverList<Observer> observers_;
};

FakeTranslateInfobarDelegate::FakeTranslateInfobarDelegate(
    const base::WeakPtr<translate::TranslateManager>& translate_manager,
    bool is_off_the_record,
    translate::TranslateStep step,
    const std::string& original_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool triggered_from_menu)
    : translate::TranslateInfoBarDelegate(translate_manager,
                                          is_off_the_record,
                                          step,
                                          original_language,
                                          target_language,
                                          error_type,
                                          triggered_from_menu) {}

void FakeTranslateInfobarDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeTranslateInfobarDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeTranslateInfobarDelegate::TriggerOnTranslateStepChanged(
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type) {
  for (auto& observer : observers_) {
    observer.OnTranslateStepChanged(step, error_type);
  }
}

// Test fixture for PlaceholderRequestCancelHandler.
class TranslateInfobarPlaceholderOverlayRequestCancelHandlerTest
    : public PlatformTest {
 public:
  TranslateInfobarPlaceholderOverlayRequestCancelHandlerTest() {
    // Set up WebState and InfoBarManager.
    web_state_.SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, std::make_unique<FakeInfobarOverlayRequestFactory>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    language::LanguagePrefs::RegisterProfilePrefs(pref_service_->registry());
    translate::TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true);
    client_ = std::make_unique<translate::testing::MockTranslateClient>(
        &driver_, pref_service_.get());
    ranker_ = std::make_unique<translate::testing::MockTranslateRanker>();
    language_model_ = std::make_unique<translate::testing::MockLanguageModel>();
    manager_ = std::make_unique<translate::TranslateManager>(
        client_.get(), ranker_.get(), language_model_.get());

    std::unique_ptr<FakeTranslateInfobarDelegate> delegate =
        std::make_unique<FakeTranslateInfobarDelegate>(
            manager_->GetWeakPtr(), false,
            translate::TRANSLATE_STEP_BEFORE_TRANSLATE, "fr", "target_language",
            translate::TranslateErrors::NONE, false);
    delegate_ = delegate.get();
    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeTranslate, std::move(delegate));
    infobar_ = infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(infobar));
  }

 protected:
  web::TestWebState web_state_;
  translate::testing::MockTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<translate::testing::MockTranslateClient> client_;
  std::unique_ptr<translate::testing::MockTranslateRanker> ranker_;
  std::unique_ptr<translate::testing::MockLanguageModel> language_model_;
  std::unique_ptr<translate::TranslateManager> manager_;
  FakeTranslateInfobarDelegate* delegate_ = nullptr;
  InfoBarIOS* infobar_ = nullptr;
};

// Test that when the translate step changes to after, then a new request is
// inserted and the placeholder request is cancelled.
TEST_F(TranslateInfobarPlaceholderOverlayRequestCancelHandlerTest,
       AfterTranslate) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<PlaceholderRequestConfig>();
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kInfobarBanner);
  std::unique_ptr<PlaceholderRequestCancelHandler> placeholder_cancel_handler =
      std::make_unique<PlaceholderRequestCancelHandler>(
          request.get(), queue,
          InfobarOverlayRequestInserter::FromWebState(&web_state_), infobar_);

  queue->AddRequest(std::move(request), std::move(placeholder_cancel_handler));

  EXPECT_EQ(1U, queue->size());
  EXPECT_TRUE(queue->front_request()->GetConfig<PlaceholderRequestConfig>());

  delegate_->TriggerOnTranslateStepChanged(
      translate::TRANSLATE_STEP_AFTER_TRANSLATE,
      translate::TranslateErrors::NONE);

  EXPECT_EQ(1U, queue->size());
  EXPECT_TRUE(queue->front_request()->GetConfig<InfobarOverlayRequestConfig>());
  EXPECT_EQ(queue->front_request()
                ->GetConfig<InfobarOverlayRequestConfig>()
                ->infobar(),
            infobar_);
}
