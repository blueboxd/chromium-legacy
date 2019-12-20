// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider_test.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_text_checking_result.h"

namespace {

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
struct HybridSpellCheckTestCase {
  size_t language_count;
  size_t enabled_language_count;
  size_t result_size;
  size_t expected_completion_count;
  size_t expected_partial_request_count;
};
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

class SpellCheckProviderCacheTest : public SpellCheckProviderTest {
 protected:
  void UpdateCustomDictionary() {
    SpellCheck* spellcheck = provider_.spellcheck();
    EXPECT_NE(spellcheck, nullptr);
    // Skip adding friend class - use public CustomDictionaryChanged from
    // |spellcheck::mojom::SpellChecker|
    static_cast<spellcheck::mojom::SpellChecker*>(spellcheck)
        ->CustomDictionaryChanged({}, {});
  }
};

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
// Test fixture for the hybrid check callback test cases.
class HybridCheckCallbackTest
    : public testing::TestWithParam<HybridSpellCheckTestCase> {
 public:
  HybridCheckCallbackTest() : provider_(&embedder_provider_) {}
  ~HybridCheckCallbackTest() override {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  spellcheck::EmptyLocalInterfaceProvider embedder_provider_;
  TestingSpellCheckProvider provider_;
};
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

TEST_F(SpellCheckProviderCacheTest, SubstringWithoutMisspellings) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(base::ASCIIToUTF16("This is a test"), last_results);
  EXPECT_TRUE(provider_.SatisfyRequestFromCache(base::ASCIIToUTF16("This is a"),
                                                &completion));
  EXPECT_EQ(result.completion_count_, 1U);
}

TEST_F(SpellCheckProviderCacheTest, SubstringWithMisspellings) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  std::vector<blink::WebTextCheckingResult> results;
  results.push_back(
      blink::WebTextCheckingResult(blink::kWebTextDecorationTypeSpelling, 5, 3,
                                   std::vector<blink::WebString>({"isq"})));
  last_results.Assign(results);
  provider_.SetLastResults(base::ASCIIToUTF16("This isq a test"), last_results);
  EXPECT_TRUE(provider_.SatisfyRequestFromCache(
      base::ASCIIToUTF16("This isq a"), &completion));
  EXPECT_EQ(result.completion_count_, 1U);
}

TEST_F(SpellCheckProviderCacheTest, ShorterTextNotSubstring) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(base::ASCIIToUTF16("This is a test"), last_results);
  EXPECT_FALSE(provider_.SatisfyRequestFromCache(
      base::ASCIIToUTF16("That is a"), &completion));
  EXPECT_EQ(result.completion_count_, 0U);
}

TEST_F(SpellCheckProviderCacheTest, ResetCacheOnCustomDictionaryUpdate) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(base::ASCIIToUTF16("This is a test"), last_results);

  UpdateCustomDictionary();

  EXPECT_FALSE(provider_.SatisfyRequestFromCache(
      base::ASCIIToUTF16("This is a"), &completion));
  EXPECT_EQ(result.completion_count_, 0U);
}

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
// Tests that the SpellCheckProvider does not call into the native spell checker
// on Windows when the native spell checker flags are disabled.
TEST_F(SpellCheckProviderTest, ShouldNotUseBrowserSpellCheck) {
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(spellcheck::kWinUseBrowserSpellChecker);

  FakeTextCheckingResult completion;
  base::string16 text = base::ASCIIToUTF16("This is a test");
  provider_.RequestTextChecking(
      text, std::make_unique<FakeTextCheckingCompletion>(&completion));

  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 0U);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
}

// Tests that the SpellCheckProvider calls into the native spell checker when
// the browser spell checker flag is enabled, but the hybrid spell checker flag
// isn't.
TEST_F(SpellCheckProviderTest, ShouldUseBrowserSpellCheck) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker()) {
    return;
  }

  base::test::ScopedFeatureList local_features;
  local_features.InitWithFeatures({spellcheck::kWinUseBrowserSpellChecker},
                                  {spellcheck::kWinUseHybridSpellChecker});

  FakeTextCheckingResult completion;
  base::string16 text = base::ASCIIToUTF16("This is a test");
  provider_.RequestTextChecking(
      text, std::make_unique<FakeTextCheckingCompletion>(&completion));

  EXPECT_EQ(provider_.spelling_service_call_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 1U);
  EXPECT_EQ(completion.completion_count_, 0U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
}

// Tests that the SpellCheckProvider calls into the native spell checker only
// when needed.
TEST_F(SpellCheckProviderTest, ShouldRequestBrowserCheckWhenNeeded) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker()) {
    return;
  }

  base::test::ScopedFeatureList local_features;
  local_features.InitWithFeatures(
      /*enabled_features=*/{spellcheck::kWinUseBrowserSpellChecker,
                            spellcheck::kWinUseHybridSpellChecker},
      /*disabled_features=*/{});
  FakeTextCheckingResult completion;

  // No languages - should go straight to completion
  provider_.spellcheck()->SetFakeLanguageCounts(0U, 0U);
  provider_.RequestTextChecking(
      base::ASCIIToUTF16("First"),
      std::make_unique<FakeTextCheckingCompletion>(&completion));

  EXPECT_EQ(provider_.spelling_service_call_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 0U);
  EXPECT_EQ(provider_.partial_text_check_requests_.size(), 0U);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(completion.cancellation_count_, 0U);

  // Added 1 disabled spell check language - should go to browser
  provider_.spellcheck()->SetFakeLanguageCounts(1U, 0U);
  provider_.RequestTextChecking(
      base::ASCIIToUTF16("Second"),
      std::make_unique<FakeTextCheckingCompletion>(&completion));

  EXPECT_EQ(provider_.spelling_service_call_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 0U);
  EXPECT_EQ(provider_.partial_text_check_requests_.size(), 1U);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(completion.cancellation_count_, 0U);

  // Enabled the only language - should go straight to completion
  provider_.spellcheck()->SetFakeLanguageCounts(1U, 1U);
  provider_.RequestTextChecking(
      base::ASCIIToUTF16("Third"),
      std::make_unique<FakeTextCheckingCompletion>(&completion));

  EXPECT_EQ(provider_.spelling_service_call_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 0U);
  EXPECT_EQ(provider_.partial_text_check_requests_.size(), 1U);
  EXPECT_EQ(completion.completion_count_, 2U);
  EXPECT_EQ(completion.cancellation_count_, 0U);

  // Added 2 more enabled languages - should go straight to completion
  provider_.spellcheck()->SetFakeLanguageCounts(3U, 3U);
  provider_.RequestTextChecking(
      base::ASCIIToUTF16("Fourth"),
      std::make_unique<FakeTextCheckingCompletion>(&completion));

  EXPECT_EQ(provider_.spelling_service_call_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 0U);
  EXPECT_EQ(provider_.partial_text_check_requests_.size(), 1U);
  EXPECT_EQ(completion.completion_count_, 3U);
  EXPECT_EQ(completion.cancellation_count_, 0U);

  // Disabled all 3 languages - should go to browser
  provider_.spellcheck()->SetFakeLanguageCounts(3U, 0U);
  provider_.RequestTextChecking(
      base::ASCIIToUTF16("Fifth"),
      std::make_unique<FakeTextCheckingCompletion>(&completion));

  EXPECT_EQ(provider_.spelling_service_call_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 0U);
  EXPECT_EQ(provider_.partial_text_check_requests_.size(), 2U);
  EXPECT_EQ(completion.completion_count_, 3U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
}

// Tests that the HybridSpellCheckParagraphComplete() callback performs the
// browser check only when needed.
INSTANTIATE_TEST_SUITE_P(
    SpellCheckProviderHybridCallbackTests,
    HybridCheckCallbackTest,
    testing::Values(
        // No languages, no results - should skip browser
        HybridSpellCheckTestCase{0U, 0U, 0U, 1U, 0U},
        // 1 disabled language, no results - should go to browser
        HybridSpellCheckTestCase{1U, 0U, 0U, 0U, 1U},
        // 1 enabled language, no results - should skip browser
        // TODO(gujen) Enable this case when b/1034043 is fixed
        // HybridSpellCheckTestCase{1U, 1U, 0U, 1U, 0U},
        // 2 disabled languages, 1 enabled, no results - should skip the browser
        // TODO(gujen) Enable this case when b/1034043 is fixed
        // HybridSpellCheckTestCase{3U, 1U, 0U, 1U, 0U},
        // 3 enabled languages, no results - should skip browser
        HybridSpellCheckTestCase{3U, 3U, 0U, 1U, 0U},
        // 3 disabled languages, no results - should go to browser
        HybridSpellCheckTestCase{3U, 0U, 0U, 0U, 1U},
        // 1 enabled language, some results - should skip browser
        HybridSpellCheckTestCase{1U, 1U, 3U, 1U, 0U},
        // 3 enabled language, some results - should skip browser
        HybridSpellCheckTestCase{3U, 3U, 2U, 1U, 0U},
        // 2 disabled languages, 1 enabled, some results - should go to browser
        HybridSpellCheckTestCase{3U, 1U, 4U, 0U, 1U}));

TEST_P(HybridCheckCallbackTest,
       HybridCallbackShouldRequestBrowserCheckWhenNeeded) {
  if (!spellcheck::WindowsVersionSupportsSpellchecker()) {
    return;
  }

  const auto& test_case = GetParam();
  base::test::ScopedFeatureList local_features;
  local_features.InitWithFeatures({spellcheck::kWinUseBrowserSpellChecker,
                                   spellcheck::kWinUseHybridSpellChecker},
                                  {});

  FakeTextCheckingResult completion;
  base::string16 text = base::ASCIIToUTF16("This is a test");

  provider_.spellcheck()->SetFakeLanguageCounts(
      test_case.language_count, test_case.enabled_language_count);
  int check_id = provider_.AddCompletionForTest(
      std::make_unique<FakeTextCheckingCompletion>(&completion));
  std::vector<SpellCheckResult> results(test_case.result_size,
                                        SpellCheckResult());

  provider_.HybridSpellCheckParagraphComplete(std::move(text), check_id,
                                              std::move(results));

  EXPECT_EQ(provider_.spelling_service_call_count_, 0U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 0U);
  EXPECT_EQ(completion.completion_count_, test_case.expected_completion_count);
  EXPECT_EQ(provider_.partial_text_check_requests_.size(),
            test_case.expected_partial_request_count);
  EXPECT_EQ(completion.cancellation_count_, 0U);
}

#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

}  // namespace
