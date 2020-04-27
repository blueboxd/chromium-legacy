// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TRANSLATE_CLIENT_IMPL_H_
#define WEBLAYER_BROWSER_TRANSLATE_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/translate_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace translate {
class LanguageState;
class TranslateManager;
}  // namespace translate

namespace weblayer {

class TranslateClientImpl
    : public translate::TranslateClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<TranslateClientImpl> {
 public:
  ~TranslateClientImpl() override;

  // Gets the LanguageState associated with the page.
  translate::LanguageState& GetLanguageState();

  // Returns the ContentTranslateDriver instance associated with this
  // WebContents.
  translate::ContentTranslateDriver* translate_driver() {
    return &translate_driver_;
  }

  // Gets the associated TranslateManager.
  translate::TranslateManager* GetTranslateManager();

  // TranslateClient implementation.
  translate::TranslateDriver* GetTranslateDriver() override;
  PrefService* GetPrefs() override;
  std::unique_ptr<translate::TranslatePrefs> GetTranslatePrefs() override;
  translate::TranslateAcceptLanguages* GetTranslateAcceptLanguages() override;
#if defined(OS_ANDROID)
  std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<translate::TranslateInfoBarDelegate> delegate)
      const override;
  int GetInfobarIconID() const override;
#endif
  bool ShowTranslateUI(translate::TranslateStep step,
                       const std::string& source_language,
                       const std::string& target_language,
                       translate::TranslateErrors::Type error_type,
                       bool triggered_from_menu) override;
  bool IsTranslatableURL(const GURL& url) override;
  void ShowReportLanguageDetectionErrorUI(const GURL& report_url) override;

 private:
  explicit TranslateClientImpl(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TranslateClientImpl>;

  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override;

  translate::ContentTranslateDriver translate_driver_;
  std::unique_ptr<translate::TranslateManager> translate_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(TranslateClientImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TRANSLATE_CLIENT_IMPL_H_
