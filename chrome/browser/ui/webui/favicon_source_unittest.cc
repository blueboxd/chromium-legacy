// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/resources/grit/ui_resources.h"

using GotDataCallback = content::URLDataSource::GotDataCallback;
using WebContentsGetter = content::ResourceRequestInfo::WebContentsGetter;
using testing::_;
using testing::Return;
using testing::ReturnArg;

void Noop(scoped_refptr<base::RefCountedMemory>) {}

class TestFaviconSource : public FaviconSource {
 public:
  // chrome::FaviconUrlFormat::kFavicon2 is arbitrary below.
  TestFaviconSource(Profile* profile, ui::NativeTheme* theme)
      : FaviconSource(profile, chrome::FaviconUrlFormat::kFavicon2),
        theme_(theme) {}

  ~TestFaviconSource() override {}

  MOCK_METHOD2(LoadIconBytes, base::RefCountedMemory*(const IconRequest&, int));

 protected:
  ui::NativeTheme* GetNativeTheme() override { return theme_; }

 private:
  ui::NativeTheme* const theme_;
};

// TODO(victorvianna): Add test coverage regarding usage of new/old format once
// FaviconRequestHandler is mockable.
class FaviconSourceTest : public testing::Test {
 public:
  FaviconSourceTest() : source_(&profile_, &theme_) {}

  void SetDarkMode(bool dark_mode) { theme_.SetDarkMode(dark_mode); }

  TestFaviconSource* source() { return &source_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ui::TestNativeTheme theme_;
  TestingProfile profile_;
  TestFaviconSource source_;
};

TEST_F(FaviconSourceTest, DarkDefault) {
  SetDarkMode(true);

  auto bytes = base::MakeRefCounted<base::RefCountedBytes>(1);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON_DARK))
      .WillOnce(Return(bytes.get()));
  source()->StartDataRequest(std::string(), WebContentsGetter(),
                             base::BindRepeating(&Noop));
}

TEST_F(FaviconSourceTest, LightDefault) {
  SetDarkMode(false);

  auto bytes = base::MakeRefCounted<base::RefCountedBytes>(1);
  EXPECT_CALL(*source(), LoadIconBytes(_, IDR_DEFAULT_FAVICON))
      .WillOnce(Return(bytes.get()));
  source()->StartDataRequest(std::string(), WebContentsGetter(),
                             base::BindRepeating(&Noop));
}
