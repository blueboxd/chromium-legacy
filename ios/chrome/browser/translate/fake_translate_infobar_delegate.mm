// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/fake_translate_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using translate::testing::MockTranslateClient;
using translate::testing::MockTranslateRanker;
using translate::testing::MockLanguageModel;

FakeTranslateInfoBarDelegate::FakeTranslateInfoBarDelegate(
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

FakeTranslateInfoBarDelegate::~FakeTranslateInfoBarDelegate() {
  for (auto& observer : observers_) {
    observer.OnTranslateInfoBarDelegateDestroyed(this);
  }
}

void FakeTranslateInfoBarDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeTranslateInfoBarDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeTranslateInfoBarDelegate::TriggerOnTranslateStepChanged(
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type) {
  for (auto& observer : observers_) {
    observer.OnTranslateStepChanged(step, error_type);
  }
}

FakeTranslateInfoBarDelegateFactory::FakeTranslateInfoBarDelegateFactory() {
  pref_service_ =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  language::LanguagePrefs::RegisterProfilePrefs(pref_service_->registry());
  translate::TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());
  pref_service_->registry()->RegisterBooleanPref(prefs::kOfferTranslateEnabled,
                                                 true);
  client_ =
      std::make_unique<MockTranslateClient>(&driver_, pref_service_.get());
  ranker_ = std::make_unique<MockTranslateRanker>();
  language_model_ = std::make_unique<MockLanguageModel>();
  manager_ = std::make_unique<translate::TranslateManager>(
      client_.get(), ranker_.get(), language_model_.get());
}

FakeTranslateInfoBarDelegateFactory::~FakeTranslateInfoBarDelegateFactory() {}

std::unique_ptr<FakeTranslateInfoBarDelegate>
FakeTranslateInfoBarDelegateFactory::CreateFakeTranslateInfoBarDelegate(
    const std::string& original_language,
    const std::string& target_language) {
  return std::make_unique<FakeTranslateInfoBarDelegate>(
      manager_->GetWeakPtr(), false,
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE,
      original_language, target_language,
      translate::TranslateErrors::Type::NONE, false);
}
