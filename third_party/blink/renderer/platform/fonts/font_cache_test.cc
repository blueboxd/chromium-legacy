// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

TEST(FontCache, getLastResortFallbackFont) {
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);
  scoped_refptr<SimpleFontData> font_data =
      font_cache->GetLastResortFallbackFont(font_description, kRetain);
  EXPECT_TRUE(font_data);

  font_description.SetGenericFamily(FontDescription::kSansSerifFamily);
  font_data = font_cache->GetLastResortFallbackFont(font_description, kRetain);
  EXPECT_TRUE(font_data);
}

TEST(FontCache, NoFallbackForPrivateUseArea) {
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);
  for (UChar32 character : {0xE000, 0xE401, 0xE402, 0xE403, 0xF8FF, 0xF0000,
                            0xFAAAA, 0x100000, 0x10AAAA}) {
    scoped_refptr<SimpleFontData> font_data =
        font_cache->FallbackFontForCharacter(font_description, character,
                                             nullptr);
    EXPECT_EQ(font_data.get(), nullptr);
  }
}

TEST(FontCache, firstAvailableOrFirst) {
  EXPECT_TRUE(FontCache::FirstAvailableOrFirst("").IsEmpty());
  EXPECT_TRUE(FontCache::FirstAvailableOrFirst(String()).IsEmpty());

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("Arial"));
  EXPECT_EQ("not exist", FontCache::FirstAvailableOrFirst("not exist"));

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("Arial, not exist"));
  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("not exist, Arial"));
  EXPECT_EQ("Arial",
            FontCache::FirstAvailableOrFirst("not exist, Arial, not exist"));

  EXPECT_EQ("not exist",
            FontCache::FirstAvailableOrFirst("not exist, not exist 2"));

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst(", not exist, Arial"));
  EXPECT_EQ("not exist",
            FontCache::FirstAvailableOrFirst(", not exist, not exist"));
}

// https://crbug.com/969402
TEST(FontCache, getLargerThanMaxUnsignedFont) {
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);
  font_description.SetComputedSize(std::numeric_limits<unsigned>::max() + 1.f);
  FontFaceCreationParams creation_params;
  scoped_refptr<blink::SimpleFontData> font_data =
      font_cache->GetFontData(font_description, AtomicString());
#if !defined(OS_ANDROID) && !defined(OS_MACOSX) && !defined(OS_WIN)
  // Unfortunately, we can't ensure a font here since on Android and Mac the
  // unittests can't access the font configuration. However, this test passes
  // when it's not crashing in FontCache.
  EXPECT_TRUE(font_data);
#endif
}

#if !defined(OS_MACOSX)
TEST(FontCache, systemFont) {
  FontCache::SystemFontFamily();
  // Test the function does not crash. Return value varies by system and config.
}
#endif

class EnumerationConsumer {
 public:
  explicit EnumerationConsumer(
      const std::vector<FontEnumerationEntry>& expectations) {
    for (const auto& f : expectations) {
      ps_name_set_.insert(f.postscript_name.Utf8());
      full_name_set_.insert(f.full_name.Utf8());
      family_set_.insert(f.family.Utf8());
    }
  }

  void Consume(const std::vector<FontEnumerationEntry>& entries) {
    for (auto f : entries) {
      ps_name_set_.erase(f.postscript_name.Utf8());
      full_name_set_.erase(f.full_name.Utf8());
      family_set_.erase(f.family.Utf8());
    }
  }

  bool AllExpectationsMet() {
    return ps_name_set_.empty() && full_name_set_.empty() &&
           family_set_.empty();
  }

 private:
  std::set<std::string> ps_name_set_;
  std::set<std::string> full_name_set_;
  std::set<std::string> family_set_;
};

TEST(FontCache, EnumerateAvailableFonts) {
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);

  std::vector<FontEnumerationEntry> expectations;

#if defined(OS_MACOSX)
  expectations.push_back(FontEnumerationEntry{"Monaco", "Monaco", "Monaco"});
  expectations.push_back(
      FontEnumerationEntry{"Menlo-Regular", "Menlo Regular", "Menlo"});
  expectations.push_back(
      FontEnumerationEntry{"Menlo-Bold", "Menlo Bold", "Menlo"});
  expectations.push_back(
      FontEnumerationEntry{"Menlo-BoldItalic", "Menlo Bold Italic", "Menlo"});
#endif

  auto entries = font_cache->EnumerateAvailableFonts();
  auto consumer = EnumerationConsumer(expectations);

  consumer.Consume(entries);
  ASSERT_TRUE(consumer.AllExpectationsMet());
}

TEST(FontCache, EnumerateAvailableFontsInvalidation) {
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);

  // Make sure we start at zero.
  font_cache->Invalidate();
  size_t zero = 0;
  ASSERT_EQ(zero, font_cache->EnumerationCacheSizeForTesting());

  // The cache gets populated.
  size_t enum_size_1 = font_cache->EnumerateAvailableFonts().size();
  ASSERT_EQ(enum_size_1, font_cache->EnumerationCacheSizeForTesting());

  // Invalidation clears the cache.
  font_cache->Invalidate();
  ASSERT_EQ(zero, font_cache->EnumerationCacheSizeForTesting());

  // The cache gets re-populated.
  size_t enum_size_2 = font_cache->EnumerateAvailableFonts().size();
  ASSERT_EQ(enum_size_1, enum_size_2);
}

}  // namespace blink
