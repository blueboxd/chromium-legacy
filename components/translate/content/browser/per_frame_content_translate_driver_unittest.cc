// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/per_frame_content_translate_driver.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

class DriverObserver : public ContentTranslateDriver::Observer {
 public:
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override {
    details_ = details;
  }

  const translate::LanguageDetectionDetails& GetObservedDetails() const {
    return details_;
  }

 private:
  translate::LanguageDetectionDetails details_;
};

class PerFrameContentTranslateDriverTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    driver_ = std::make_unique<PerFrameContentTranslateDriver>(
        &(web_contents()->GetController()),
        nullptr /* url_language_histogram */);
    driver_->AddObserver(&observer_);
  }

  void TearDown() override {
    driver_->RemoveObserver(&observer_);
    driver_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void OnWebLanguageDetectionDetails(const std::string& content_language,
                                     const std::string& html_lang,
                                     const GURL& url,
                                     bool has_no_translate_meta) {
    driver_->OnWebLanguageDetectionDetails(content_language, html_lang, url,
                                           has_no_translate_meta);
  }

  void OnPageContents(const base::string16& contents) {
    driver_->OnPageContents(base::TimeTicks::Now(), contents);
  }

  const std::string& GetAdoptedLanguage() const {
    return observer_.GetObservedDetails().adopted_language;
  }

  bool HasGoodContentDetection() const {
    return observer_.GetObservedDetails().is_cld_reliable;
  }

  bool DoNotTranslate() const {
    return observer_.GetObservedDetails().has_notranslate;
  }

 private:
  std::unique_ptr<PerFrameContentTranslateDriver> driver_;
  DriverObserver observer_;
};

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_MetaTagOverridesMinimalContent) {
  base::string16 contents =
      base::UTF8ToUTF16("El niño atrapó un dorado muy grande con cebo vivo.");
  OnPageContents(contents);
  OnWebLanguageDetectionDetails("en" /* meta */, "" /* html */,
                                GURL("https://whatever.com"),
                                false /* notranslate */);
  EXPECT_FALSE(DoNotTranslate());
  EXPECT_FALSE(HasGoodContentDetection());
  EXPECT_EQ("en", GetAdoptedLanguage());
}

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_HtmlLangOverridesMetaTag) {
  base::string16 contents =
      base::UTF8ToUTF16("El niño atrapó un dorado muy grande con cebo vivo.");
  OnPageContents(contents);
  OnWebLanguageDetectionDetails("en" /* meta */, "fr" /* html */,
                                GURL("https://whatever.com"),
                                false /* notranslate */);
  EXPECT_EQ("fr", GetAdoptedLanguage());
}

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_SufficientContentOverridesMetaTag) {
  base::string16 contents = base::UTF8ToUTF16(
      "El niño atrapó un dorado muy grande con cebo vivo. Fileteó el "
      "pescado y lo asó a la parrilla. Sabía excelente. Espera pescar otro "
      "buen pescado mañana.");
  OnPageContents(contents);
  OnWebLanguageDetectionDetails("en" /* meta */, "" /* html */,
                                GURL("https://whatever.com"),
                                false /* notranslate */);
  EXPECT_TRUE(HasGoodContentDetection());
  EXPECT_EQ("es", GetAdoptedLanguage());
}

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_SufficientContentOverridesHtmlLang) {
  base::string16 contents = base::UTF8ToUTF16(
      "El niño atrapó un dorado muy grande con cebo vivo. Fileteó el "
      "pescado y lo asó a la parrilla. Sabía excelente. Espera pescar otro "
      "buen pescado mañana.");
  OnPageContents(contents);
  OnWebLanguageDetectionDetails("en" /* meta */, "es-MX" /* html */,
                                GURL("https://whatever.com"),
                                false /* notranslate */);
  EXPECT_EQ("es", GetAdoptedLanguage());
}

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_NoTranslateMetaTag) {
  base::string16 contents = base::UTF8ToUTF16(
      "El niño atrapó un dorado muy grande con cebo vivo. Fileteó el "
      "pescado y lo asó a la parrilla. Sabía excelente. Espera pescar otro "
      "buen pescado mañana.");
  OnPageContents(contents);
  OnWebLanguageDetectionDetails("en" /* meta */, "" /* html */,
                                GURL("https://whatever.com"),
                                true /* notranslate */);
  EXPECT_TRUE(DoNotTranslate());
  EXPECT_EQ("es", GetAdoptedLanguage());
}

TEST_F(PerFrameContentTranslateDriverTest,
       ComputeActualPageLanguage_LanguageFormatVariants) {
  OnPageContents(base::UTF8ToUTF16("Some content"));
  OnWebLanguageDetectionDetails("ZH_tw" /* meta */, "" /* html */,
                                GURL("https://whatever.com"),
                                false /* notranslate */);
  EXPECT_EQ("zh-TW", GetAdoptedLanguage());

  OnPageContents(base::UTF8ToUTF16("Some other content"));
  OnWebLanguageDetectionDetails(" fr , es,en " /* meta */, "" /* html */,
                                GURL("https://whatever.com"),
                                false /* notranslate */);
  EXPECT_EQ("fr", GetAdoptedLanguage());
}

}  // namespace translate
