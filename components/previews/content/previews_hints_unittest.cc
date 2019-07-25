// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_hints.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/optimization_guide/bloom_filter.h"
#include "components/optimization_guide/hint_cache.h"
#include "components/optimization_guide/hint_cache_store.h"
#include "components/optimization_guide/hint_update_data.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/host_filter.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto_database_provider_test_base.h"
#include "components/previews/core/previews_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

const int kBlackBlacklistBloomFilterNumHashFunctions = 7;
const int kBlackBlacklistBloomFilterNumBits = 511;

void PopulateBlackBlacklistBloomFilter(
    optimization_guide::BloomFilter* bloom_filter) {
  bloom_filter->Add("black.com");
}

void AddBlacklistBloomFilterToConfig(
    const optimization_guide::BloomFilter& blacklist_bloom_filter,
    int num_hash_functions,
    int num_bits,
    optimization_guide::proto::Configuration* config) {
  std::string blacklist_data((char*)&blacklist_bloom_filter.bytes()[0],
                             blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config->add_optimization_blacklists();
  blacklist_proto->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
}

}  // namespace

class TestHostFilter : public optimization_guide::HostFilter {
 public:
  explicit TestHostFilter(std::string single_host_match)
      : HostFilter(nullptr), single_host_match_(single_host_match) {}

  bool ContainsHostSuffix(const GURL& url) const override {
    return single_host_match_ == url.host();
  }

 private:
  std::string single_host_match_;
};

class PreviewsHintsTest
    : public optimization_guide::ProtoDatabaseProviderTestBase {
 public:
  PreviewsHintsTest() {}

  ~PreviewsHintsTest() override {}

  void SetUp() override {
    ProtoDatabaseProviderTestBase::SetUp();
    hint_cache_ = std::make_unique<optimization_guide::HintCache>(
        std::make_unique<optimization_guide::HintCacheStore>(
            db_provider_.get(), temp_dir_.GetPath(), nullptr /* pref_service */,
            scoped_task_environment_.GetMainThreadTaskRunner()));

    is_store_initialized_ = false;
    hint_cache_->Initialize(
        false /*=purge_existing_data*/,
        base::BindOnce(&PreviewsHintsTest::OnStoreInitialized,
                       base::Unretained(this)));
    while (!is_store_initialized_) {
      RunUntilIdle();
    }
  }

  void TearDown() override {
    ProtoDatabaseProviderTestBase::TearDown();
    previews_hints_.reset();
    RunUntilIdle();
    hint_cache_.reset();
    RunUntilIdle();
  }

  void ParseConfig(const optimization_guide::proto::Configuration& config) {
    optimization_guide::HintsComponentInfo info(
        base::Version("1.0"),
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("somefile.pb")));
    ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(config, info.path));
    previews_hints_ = PreviewsHints::CreateFromHintsComponent(
        info,
        hint_cache_->MaybeCreateUpdateDataForComponentHints(info.version));

    are_previews_hints_initialized_ = false;
    previews_hints_->Initialize(
        hint_cache_.get(),
        base::BindOnce(&PreviewsHintsTest::OnPreviewsHintsInitialized,
                       base::Unretained(this)));
    while (!are_previews_hints_initialized_) {
      RunUntilIdle();
    }
  }

  PreviewsHints* previews_hints() { return previews_hints_.get(); }

  optimization_guide::HintCache* hint_cache() { return hint_cache_.get(); }

  bool HasLitePageRedirectBlacklist() {
    return previews_hints_->lite_page_redirect_blacklist_.get() != nullptr;
  }

 protected:
  bool MaybeLoadHintAndCheckIsWhitelisted(
      const GURL& url,
      PreviewsType type,
      int* out_inflation_percent,
      net::EffectiveConnectionType* out_ect_threshold,
      std::string* serialized_hint_version_string);

  void MaybeLoadHintAndLogHintCacheMatch(const GURL& url,
                                         bool is_committed,
                                         net::EffectiveConnectionType ect);

  void RunUntilIdle() {
    scoped_task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 private:
  void OnStoreInitialized() { is_store_initialized_ = true; }
  void OnPreviewsHintsInitialized() { are_previews_hints_initialized_ = true; }
  void OnHintLoaded(const optimization_guide::proto::Hint* /*unused*/) {
    is_hint_loaded_ = true;
  }

  void WriteConfigToFile(const optimization_guide::proto::Configuration& config,
                         const base::FilePath& filePath) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_EQ(static_cast<int32_t>(serialized_config.length()),
              base::WriteFile(filePath, serialized_config.data(),
                              serialized_config.length()));
  }

  void MaybeLoadHint(const GURL& url);

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  bool is_store_initialized_;
  bool are_previews_hints_initialized_;
  bool is_hint_loaded_;

  std::unique_ptr<optimization_guide::HintCache> hint_cache_;
  std::unique_ptr<PreviewsHints> previews_hints_;
};

bool PreviewsHintsTest::MaybeLoadHintAndCheckIsWhitelisted(
    const GURL& url,
    PreviewsType type,
    int* out_inflation_percent,
    net::EffectiveConnectionType* out_ect_threshold,
    std::string* out_serialized_hint_version_string) {
  MaybeLoadHint(url);
  return previews_hints_->IsWhitelisted(url, type, out_inflation_percent,
                                        out_ect_threshold,
                                        out_serialized_hint_version_string);
}

void PreviewsHintsTest::MaybeLoadHintAndLogHintCacheMatch(
    const GURL& url,
    bool is_committed,
    net::EffectiveConnectionType ect) {
  MaybeLoadHint(url);
  previews_hints_->LogHintCacheMatch(url, is_committed, ect);
}

void PreviewsHintsTest::MaybeLoadHint(const GURL& url) {
  is_hint_loaded_ = false;
  if (previews_hints_->MaybeLoadOptimizationHints(
          url, base::BindOnce(&PreviewsHintsTest::OnHintLoaded,
                              base::Unretained(this)))) {
    while (!is_hint_loaded_) {
      RunUntilIdle();
    }
  }
}

// NOTE: most of the PreviewsHints tests are still included in the tests for
// PreviewsOptimizationGuide.

TEST_F(PreviewsHintsTest, LogHintCacheMatch) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint1 =
      optimization1->add_resource_loading_hints();
  resource_loading_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint1->set_resource_pattern("news_cruft.js");
  ParseConfig(config);

  base::HistogramTester histogram_tester;

  // First verify no histogram counts for non-matching URL host.
  MaybeLoadHintAndLogHintCacheMatch(
      GURL("https://someotherdomain.com/news/story.html"),
      false /* is_committed */, net::EFFECTIVE_CONNECTION_TYPE_3G);
  MaybeLoadHintAndLogHintCacheMatch(
      GURL("https://someotherdomain.com/news/story2.html"),
      true /* is_committed */, net::EFFECTIVE_CONNECTION_TYPE_4G);
  histogram_tester.ExpectTotalCount(
      "Previews.OptimizationGuide.HintCache.HasHint.BeforeCommit", 0);
  histogram_tester.ExpectTotalCount(
      "Previews.OptimizationGuide.HintCache.HasHint.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "Previews.OptimizationGuide.HintCache.HintLoaded.AtCommit", 0);
  histogram_tester.ExpectTotalCount(
      "Previews.OptimizationGuide.HintCache.PageMatch.AtCommit", 0);

  // Now verify do have histogram counts for matching URL host.
  MaybeLoadHintAndLogHintCacheMatch(
      GURL("https://somedomain.org/news/story.html"), false /* is_committed */,
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  MaybeLoadHintAndLogHintCacheMatch(
      GURL("https://somedomain.org/news/story2.html"), true /* is_committed */,
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  histogram_tester.ExpectBucketCount(
      "Previews.OptimizationGuide.HintCache.HasHint.BeforeCommit",
      4 /* EFFECTIVE_CONNECTION_TYPE_3G */, 1);
  histogram_tester.ExpectBucketCount(
      "Previews.OptimizationGuide.HintCache.HasHint.AtCommit",
      5 /* EFFECTIVE_CONNECTION_TYPE_4G */, 1);
  histogram_tester.ExpectBucketCount(
      "Previews.OptimizationGuide.HintCache.HostMatch.AtCommit",
      5 /* EFFECTIVE_CONNECTION_TYPE_4G */, 1);
  histogram_tester.ExpectBucketCount(
      "Previews.OptimizationGuide.HintCache.PageMatch.AtCommit",
      5 /* EFFECTIVE_CONNECTION_TYPE_4G */, 1);
}

TEST_F(PreviewsHintsTest, IsBlacklistedReturnsTrueIfNoBloomFilter) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);

  optimization_guide::proto::Configuration config;
  ParseConfig(config);

  EXPECT_FALSE(HasLitePageRedirectBlacklist());

  EXPECT_FALSE(previews_hints()->IsBlacklisted(GURL("https://black.com/path"),
                                               PreviewsType::OFFLINE));

  EXPECT_TRUE(previews_hints()->IsBlacklisted(
      GURL("https://black.com/path"), PreviewsType::LITE_PAGE_REDIRECT));
  EXPECT_TRUE(previews_hints()->IsBlacklisted(
      GURL("https://joe.black.com/path"), PreviewsType::LITE_PAGE_REDIRECT));
  EXPECT_TRUE(previews_hints()->IsBlacklisted(
      GURL("https://nonblack.com"), PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsHintsTest, IsBlacklisted) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);

  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);

  optimization_guide::proto::Configuration config;
  AddBlacklistBloomFilterToConfig(blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ParseConfig(config);

  EXPECT_TRUE(HasLitePageRedirectBlacklist());
  EXPECT_FALSE(previews_hints()->IsBlacklisted(GURL("https://black.com/path"),
                                               PreviewsType::OFFLINE));
  EXPECT_TRUE(previews_hints()->IsBlacklisted(
      GURL("https://black.com/path"), PreviewsType::LITE_PAGE_REDIRECT));
  EXPECT_TRUE(previews_hints()->IsBlacklisted(
      GURL("https://joe.black.com/path"), PreviewsType::LITE_PAGE_REDIRECT));
  EXPECT_FALSE(previews_hints()->IsBlacklisted(
      GURL("https://nonblack.com"), PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsHintsTest, ParseConfigWithInsufficientConfigDetails) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);

  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);

  // Set num_bits in config to one more than the size of the data.
  int num_bits = blacklist_bloom_filter.bytes().size() * 8 + 1;

  optimization_guide::proto::Configuration config;
  AddBlacklistBloomFilterToConfig(blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  num_bits, &config);
  ParseConfig(config);

  EXPECT_FALSE(HasLitePageRedirectBlacklist());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFoundServerBlacklistConfig,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::
          kFailedServerBlacklistBadConfig,
      1);

  EXPECT_TRUE(previews_hints()->IsBlacklisted(
      GURL("https://black.com/path"), PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsHintsTest, ParseConfigWithTooLargeBlacklist) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);

  int too_many_bits =
      previews::params::LitePageRedirectPreviewMaxServerBlacklistByteSize() *
          8 +
      1;

  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions, too_many_bits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);

  optimization_guide::proto::Configuration config;
  AddBlacklistBloomFilterToConfig(blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  too_many_bits, &config);
  ParseConfig(config);

  EXPECT_FALSE(HasLitePageRedirectBlacklist());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFoundServerBlacklistConfig,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::
          kFailedServerBlacklistTooBig,
      1);

  EXPECT_TRUE(previews_hints()->IsBlacklisted(
      GURL("https://black.com/path"), PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsHintsTest, IsWhitelistedOutParams) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);
  optimization_guide::proto::Configuration config;

  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");

  // Page hint for "/has_inflation_percent/"
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/has_inflation_percent/");
  optimization_guide::proto::Optimization* optimization_with_inflation_percent =
      page_hint1->add_whitelisted_optimizations();
  optimization_with_inflation_percent->set_inflation_percent(55);
  optimization_with_inflation_percent->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint1 =
      optimization_with_inflation_percent->add_resource_loading_hints();
  resource_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint1->set_resource_pattern("resource1.js");
  // Page hint for "/has_max_ect_trigger/"
  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("/has_max_ect_trigger/");
  page_hint2->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_4G);
  optimization_guide::proto::Optimization*
      optimization_without_inflation_percent =
          page_hint2->add_whitelisted_optimizations();
  optimization_without_inflation_percent->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint2a =
      optimization_without_inflation_percent->add_resource_loading_hints();
  resource_hint2a->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint2a->set_resource_pattern("resource2a.js");
  optimization_guide::proto::ResourceLoadingHint* unsupported_resource_hint =
      optimization_without_inflation_percent->add_resource_loading_hints();
  unsupported_resource_hint->set_loading_optimization_type(
      optimization_guide::proto::LOADING_UNSPECIFIED);
  unsupported_resource_hint->set_resource_pattern(
      "unsupported_resource_hint.js");
  optimization_guide::proto::ResourceLoadingHint* resource_hint2b =
      optimization_without_inflation_percent->add_resource_loading_hints();
  resource_hint2b->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint2b->set_resource_pattern("resource2b.js");
  // Page hint for "/has_metadata/" with all details provided in metadata and
  // should take precedence over the details at the top-level.
  optimization_guide::proto::PageHint* page_hint3 = hint1->add_page_hints();
  page_hint3->set_page_pattern("/has_metadata/");
  page_hint3->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_4G);
  optimization_guide::proto::Optimization* optimization_with_metadata =
      page_hint3->add_whitelisted_optimizations();
  optimization_with_metadata->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_with_metadata->set_inflation_percent(12345);
  optimization_guide::proto::ResourceLoadingHint* unused_resource_hint =
      optimization_with_metadata->add_resource_loading_hints();
  unused_resource_hint->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  unused_resource_hint->set_resource_pattern("unused_resource_hint.js");
  optimization_guide::proto::PreviewsMetadata* previews_metadata =
      optimization_with_metadata->mutable_previews_metadata();
  previews_metadata->set_inflation_percent(123);
  optimization_guide::proto::ResourceLoadingHint* resource_hint3 =
      previews_metadata->add_resource_loading_hints();
  resource_hint3->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint3->set_resource_pattern("resource3.js");
  previews_metadata->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_3G);

  ParseConfig(config);

  // Verify optimization providing inflation_percent and hints version.
  int inflation_percent = 0;
  net::EffectiveConnectionType ect_threshold =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  std::string serialized_hint_version_string;
  EXPECT_TRUE(MaybeLoadHintAndCheckIsWhitelisted(
      GURL("https://www.somedomain.org/has_inflation_percent/"),
      PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent, &ect_threshold,
      &serialized_hint_version_string));
  EXPECT_EQ(55, inflation_percent);
  EXPECT_EQ(net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            ect_threshold);
  EXPECT_EQ("someversion", serialized_hint_version_string);

  // Verify page hint providing ECT trigger.
  inflation_percent = 0;
  ect_threshold =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  serialized_hint_version_string = "";
  EXPECT_TRUE(MaybeLoadHintAndCheckIsWhitelisted(
      GURL("https://www.somedomain.org/has_max_ect_trigger/"),
      PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent, &ect_threshold,
      &serialized_hint_version_string));
  EXPECT_EQ(0, inflation_percent);
  EXPECT_EQ(net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G,
            ect_threshold);
  EXPECT_EQ("someversion", serialized_hint_version_string);

  // Verify getting resource patterns to block.
  std::vector<std::string> patterns_to_block1;
  previews_hints()->GetResourceLoadingHints(
      GURL("https://www.somedomain.org/has_inflation_percent/"),
      &patterns_to_block1);
  EXPECT_EQ(1ul, patterns_to_block1.size());
  EXPECT_EQ("resource1.js", patterns_to_block1[0]);
  std::vector<std::string> patterns_to_block2;
  previews_hints()->GetResourceLoadingHints(
      GURL("https://www.somedomain.org/has_max_ect_trigger/"),
      &patterns_to_block2);
  EXPECT_EQ(2ul, patterns_to_block2.size());
  EXPECT_EQ("resource2a.js", patterns_to_block2[0]);
  EXPECT_EQ("resource2b.js", patterns_to_block2[1]);

  // Verify page hint having data stored in metadata.
  inflation_percent = 0;
  ect_threshold =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  serialized_hint_version_string = "";
  EXPECT_TRUE(MaybeLoadHintAndCheckIsWhitelisted(
      GURL("https://www.somedomain.org/has_metadata/"),
      PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent, &ect_threshold,
      &serialized_hint_version_string));
  EXPECT_EQ(123, inflation_percent);
  std::vector<std::string> patterns_to_block3;
  previews_hints()->GetResourceLoadingHints(
      GURL("https://www.somedomain.org/has_metadata/"), &patterns_to_block3);
  EXPECT_EQ(1ul, patterns_to_block3.size());
  EXPECT_EQ("resource3.js", patterns_to_block3[0]);
  EXPECT_EQ(net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G,
            ect_threshold);
}

TEST_F(PreviewsHintsTest,
       IsWhitelistedForSecondOptimizationDeferAllScriptWithFirstDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures({features::kDeferAllScriptPreviews},
                               {features::kResourceLoadingHints});
  optimization_guide::proto::Configuration config;

  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");

  // Page hint with RESOURCE_LOADING and DEFER_ALL_SCRIPT optimizations.
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/has_multiple_optimizations/");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::Optimization* optimization2 =
      page_hint1->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::DEFER_ALL_SCRIPT);

  ParseConfig(config);

  int inflation_percent = 0;
  net::EffectiveConnectionType ect_threshold =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  std::string serialized_hint_version_string;
  EXPECT_TRUE(MaybeLoadHintAndCheckIsWhitelisted(
      GURL("https://www.somedomain.org/has_multiple_optimizations/"),
      PreviewsType::DEFER_ALL_SCRIPT, &inflation_percent, &ect_threshold,
      &serialized_hint_version_string));
  EXPECT_FALSE(MaybeLoadHintAndCheckIsWhitelisted(
      GURL("https://www.somedomain.org/has_multiple_optimizations/"),
      PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent, &ect_threshold,
      &serialized_hint_version_string));
}

TEST_F(PreviewsHintsTest,
       IsWhitelistedForSecondOptimizationResourceLoadingWithFirstDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures({features::kResourceLoadingHints},
                               {features::kDeferAllScriptPreviews});
  optimization_guide::proto::Configuration config;

  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");

  // Page hint with RESOURCE_LOADING and DEFER_ALL_SCRIPT optimizations.
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/has_multiple_optimizations/");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::DEFER_ALL_SCRIPT);
  optimization_guide::proto::Optimization* optimization2 =
      page_hint1->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  ParseConfig(config);

  int inflation_percent = 0;
  net::EffectiveConnectionType ect_threshold =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  std::string serialized_hint_version_string;
  EXPECT_TRUE(MaybeLoadHintAndCheckIsWhitelisted(
      GURL("https://www.somedomain.org/has_multiple_optimizations/"),
      PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent, &ect_threshold,
      &serialized_hint_version_string));
  EXPECT_FALSE(MaybeLoadHintAndCheckIsWhitelisted(
      GURL("https://www.somedomain.org/has_multiple_optimizations/"),
      PreviewsType::NOSCRIPT, &inflation_percent, &ect_threshold,
      &serialized_hint_version_string));
}

TEST_F(PreviewsHintsTest, IsWhitelistedForExperimentalPreview) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);
  optimization_guide::proto::Configuration config;

  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");

  // Page hint for "/experimental_preview/"
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/experimental_preview/");
  page_hint1->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_3G);

  // First add experimental PageHint optimization.
  optimization_guide::proto::Optimization* experimental_optimization =
      page_hint1->add_whitelisted_optimizations();
  experimental_optimization->set_experiment_name("foo_experiment");
  experimental_optimization->set_inflation_percent(99);
  experimental_optimization->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* experimental_resourcehint =
      experimental_optimization->add_resource_loading_hints();
  experimental_resourcehint->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  experimental_resourcehint->set_resource_pattern("experimental_resource.js");

  // Add a non-experimental PageHint optimization with same resource pattern.
  optimization_guide::proto::Optimization* default_pagehint_optimization =
      page_hint1->add_whitelisted_optimizations();
  default_pagehint_optimization->set_inflation_percent(33);
  default_pagehint_optimization->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* default_resourcehint =
      default_pagehint_optimization->add_resource_loading_hints();
  default_resourcehint->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  default_resourcehint->set_resource_pattern("default_resource.js");

  ParseConfig(config);

  // Test with the experiment disabled.
  {
    // Verify default resource hint whitelisted (via inflation_percent).
    int inflation_percent = 0;
    net::EffectiveConnectionType ect_threshold =
        net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
    std::string serialized_hint_version_string;
    EXPECT_TRUE(MaybeLoadHintAndCheckIsWhitelisted(
        GURL("https://www.somedomain.org/experimental_preview/"
             "experimental_resource.js"),
        PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent,
        &ect_threshold, &serialized_hint_version_string));
    EXPECT_EQ(33, inflation_percent);
    EXPECT_EQ(net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G,
              ect_threshold);
    EXPECT_EQ("someversion", serialized_hint_version_string);

    std::vector<std::string> patterns_to_block;
    previews_hints()->GetResourceLoadingHints(
        GURL("https://www.somedomain.org/experimental_preview/"),
        &patterns_to_block);
    EXPECT_EQ(1ul, patterns_to_block.size());
    EXPECT_EQ("default_resource.js", patterns_to_block[0]);
  }

  // Now enable the experiment and verify experimental resource hint chosen.
  {
    base::test::ScopedFeatureList scoped_list2;
    scoped_list2.InitAndEnableFeatureWithParameters(
        optimization_guide::features::kOptimizationHintsExperiments,
        {{"experiment_name", "foo_experiment"}});

    int inflation_percent = 0;
    net::EffectiveConnectionType ect_threshold =
        net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G;
    std::string serialized_hint_version_string;
    EXPECT_TRUE(MaybeLoadHintAndCheckIsWhitelisted(
        GURL("https://www.somedomain.org/experimental_preview/"
             "experimental_resource.js"),
        PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent,
        &ect_threshold, &serialized_hint_version_string));
    EXPECT_EQ(99, inflation_percent);
    EXPECT_EQ(net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G,
              ect_threshold);
    EXPECT_EQ("someversion", serialized_hint_version_string);

    std::vector<std::string> patterns_to_block;
    previews_hints()->GetResourceLoadingHints(
        GURL("https://www.somedomain.org/experimental_preview/"),
        &patterns_to_block);
    EXPECT_EQ(1ul, patterns_to_block.size());
    EXPECT_EQ("experimental_resource.js", patterns_to_block[0]);
  }
}

TEST_F(PreviewsHintsTest, IsWhitelistedForNoopExperimentalPreview) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);
  optimization_guide::proto::Configuration config;

  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");

  // Page hint for "/experimental_preview/"
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/experimental_preview/");
  page_hint1->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_3G);

  // First add OPTIMIZATION_NONE as no-op experimental PageHint optimization.
  optimization_guide::proto::Optimization* experimental_optimization =
      page_hint1->add_whitelisted_optimizations();
  experimental_optimization->set_experiment_name("foo_experiment");
  experimental_optimization->set_optimization_type(
      optimization_guide::proto::OPTIMIZATION_NONE);

  // Add a non-experimental PageHint optimization with same resource pattern.
  optimization_guide::proto::Optimization* default_pagehint_optimization =
      page_hint1->add_whitelisted_optimizations();
  default_pagehint_optimization->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* default_resourcehint =
      default_pagehint_optimization->add_resource_loading_hints();
  default_resourcehint->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  default_resourcehint->set_resource_pattern("default_resource.js");

  ParseConfig(config);

  // Test with the experiment disabled.
  {
    // Verify resource loading hints whitelisted.
    int inflation_percent = 0;
    net::EffectiveConnectionType ect_threshold =
        net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
    std::string serialized_hint_version_string;
    EXPECT_TRUE(MaybeLoadHintAndCheckIsWhitelisted(
        GURL("https://www.somedomain.org/experimental_preview/"
             "experimental_resource.js"),
        PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent,
        &ect_threshold, &serialized_hint_version_string));
  }

  // Now enable the experiment and verify experimental no-op screens the
  // resource loading hints from being whitelisted.
  {
    base::test::ScopedFeatureList scoped_list2;
    scoped_list2.InitAndEnableFeatureWithParameters(
        optimization_guide::features::kOptimizationHintsExperiments,
        {{"experiment_name", "foo_experiment"}});

    int inflation_percent = 0;
    net::EffectiveConnectionType ect_threshold =
        net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G;
    std::string serialized_hint_version_string;
    EXPECT_FALSE(MaybeLoadHintAndCheckIsWhitelisted(
        GURL("https://www.somedomain.org/experimental_preview/"
             "experimental_resource.js"),
        PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent,
        &ect_threshold, &serialized_hint_version_string));
  }
}

TEST_F(PreviewsHintsTest, IsWhitelistedForExcludedExperimentalPreview) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);
  optimization_guide::proto::Configuration config;

  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");

  // Page hint for "/experimental_preview/"
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/experimental_preview/");

  // First add PageHint optimization excluded from experiment.
  optimization_guide::proto::Optimization* optimization =
      page_hint1->add_whitelisted_optimizations();
  optimization->set_excluded_experiment_name("foo_experiment");
  optimization->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resourcehint =
      optimization->add_resource_loading_hints();
  resourcehint->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resourcehint->set_resource_pattern("excluded_experiment_resource.js");

  // Add a fall-through PageHint optimization with same resource pattern.
  optimization_guide::proto::Optimization* fallthrough_pagehint_optimization =
      page_hint1->add_whitelisted_optimizations();
  fallthrough_pagehint_optimization->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* fallthrough_resourcehint =
      fallthrough_pagehint_optimization->add_resource_loading_hints();
  fallthrough_resourcehint->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  fallthrough_resourcehint->set_resource_pattern("fallthrough_resource.js");

  ParseConfig(config);

  int inflation_percent = 0;
  net::EffectiveConnectionType ect_threshold =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  std::string serialized_hint_version_string;

  // Test with the experiment disabled and verify first optimization allowed.
  {
    EXPECT_TRUE(MaybeLoadHintAndCheckIsWhitelisted(
        GURL("https://www.somedomain.org/experimental_preview/"
             "not_excluded_case"),
        PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent,
        &ect_threshold, &serialized_hint_version_string));

    std::vector<std::string> patterns_to_block;
    previews_hints()->GetResourceLoadingHints(
        GURL("https://www.somedomain.org/experimental_preview/"),
        &patterns_to_block);
    EXPECT_EQ(1ul, patterns_to_block.size());
    EXPECT_EQ("excluded_experiment_resource.js", patterns_to_block[0]);
  }

  // Now enable the experiment and verify first optimization excluded.
  {
    base::test::ScopedFeatureList scoped_list2;
    scoped_list2.InitAndEnableFeatureWithParameters(
        optimization_guide::features::kOptimizationHintsExperiments,
        {{"experiment_name", "foo_experiment"}});

    EXPECT_TRUE(MaybeLoadHintAndCheckIsWhitelisted(
        GURL("https://www.somedomain.org/experimental_preview/"
             "fallthrough_case"),
        PreviewsType::RESOURCE_LOADING_HINTS, &inflation_percent,
        &ect_threshold, &serialized_hint_version_string));

    std::vector<std::string> patterns_to_block;
    previews_hints()->GetResourceLoadingHints(
        GURL("https://www.somedomain.org/experimental_preview/"),
        &patterns_to_block);
    EXPECT_EQ(1ul, patterns_to_block.size());
    EXPECT_EQ("fallthrough_resource.js", patterns_to_block[0]);
  }
}

TEST_F(PreviewsHintsTest, ParseDuplicateConfigs) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint1 =
      optimization1->add_resource_loading_hints();
  resource_loading_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint1->set_resource_pattern("news_cruft.js");

  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    ParseConfig(config);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        static_cast<int>(
            optimization_guide::ProcessHintsComponentResult::kSuccess),
        1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as a duplicate version.
  {
    base::HistogramTester histogram_tester;
    ParseConfig(config);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.ProcessHintsResult",
        static_cast<int>(optimization_guide::ProcessHintsComponentResult::
                             kSkippedProcessingHints),
        1);
  }
}

TEST_F(PreviewsHintsTest, ComponentInfoDidNotProduceConfig) {
  base::HistogramTester histogram_tester;

  optimization_guide::HintsComponentInfo info(base::Version("1.0"),
                                              base::FilePath());
  std::unique_ptr<PreviewsHints> previews_hints =
      PreviewsHints::CreateFromHintsComponent(
          info,
          hint_cache()->MaybeCreateUpdateDataForComponentHints(info.version));

  EXPECT_EQ(nullptr, previews_hints);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ProcessHintsResult",
      static_cast<int>(optimization_guide::ProcessHintsComponentResult::
                           kFailedInvalidParameters),
      1);
}

}  // namespace previews
