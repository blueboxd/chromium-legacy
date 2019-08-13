// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"

#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/bloom_filter.h"
#include "components/optimization_guide/command_line_top_host_provider.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto_database_provider_test_base.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_browser_thread_bundle.h"

namespace {

const int kBlackBlacklistBloomFilterNumHashFunctions = 7;
const int kBlackBlacklistBloomFilterNumBits = 511;

void PopulateBlackBlacklistBloomFilter(
    optimization_guide::BloomFilter* bloom_filter) {
  bloom_filter->Add("black.com");
}

void AddBlacklistBloomFilterToConfig(
    optimization_guide::proto::OptimizationType optimization_type,
    const optimization_guide::BloomFilter& blacklist_bloom_filter,
    int num_hash_functions,
    int num_bits,
    optimization_guide::proto::Configuration* config) {
  std::string blacklist_data(
      reinterpret_cast<const char*>(&blacklist_bloom_filter.bytes()[0]),
      blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config->add_optimization_blacklists();
  blacklist_proto->set_optimization_type(optimization_type);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
}

}  // namespace

class TestOptimizationGuideService
    : public optimization_guide::OptimizationGuideService {
 public:
  explicit TestOptimizationGuideService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : OptimizationGuideService(ui_task_runner) {}

  ~TestOptimizationGuideService() override = default;

  void AddObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    add_observer_called_ = true;
  }

  void RemoveObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    remove_observer_called_ = true;
  }

  bool AddObserverCalled() const { return add_observer_called_; }
  bool RemoveObserverCalled() const { return remove_observer_called_; }

 private:
  bool add_observer_called_ = false;
  bool remove_observer_called_ = false;
};

class OptimizationGuideHintsManagerTest
    : public optimization_guide::ProtoDatabaseProviderTestBase {
 public:
  OptimizationGuideHintsManagerTest() = default;
  ~OptimizationGuideHintsManagerTest() override = default;

  void SetUp() override {
    optimization_guide::ProtoDatabaseProviderTestBase::SetUp();
    CreateServiceAndHintsManager();
  }

  void TearDown() override {
    optimization_guide::ProtoDatabaseProviderTestBase::TearDown();
    ResetHintsManager();
  }

  void CreateServiceAndHintsManager(
      optimization_guide::TopHostProvider* top_host_provider = nullptr) {
    if (hints_manager_) {
      ResetHintsManager();
    }
    optimization_guide_service_ =
        std::make_unique<TestOptimizationGuideService>(
            browser_thread_bundle_.GetMainThreadTaskRunner());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    optimization_guide::prefs::RegisterProfilePrefs(pref_service_->registry());

    hints_manager_ = std::make_unique<OptimizationGuideHintsManager>(
        optimization_guide_service_.get(), temp_dir(), pref_service_.get(),
        db_provider_.get(), top_host_provider);

    // Add observer is called after the HintCache is fully initialized,
    // indicating that the OptimizationGuideHintsManager is ready to process
    // hints.
    while (!optimization_guide_service_->AddObserverCalled()) {
      RunUntilIdle();
    }
  }

  void ResetHintsManager() {
    hints_manager_.reset();
    RunUntilIdle();
  }

  void ProcessInvalidHintsComponentInfo(const std::string& version) {
    optimization_guide::HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("notaconfigfile")));

    base::RunLoop run_loop;
    hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager_->OnHintsComponentAvailable(info);
    run_loop.Run();
  }

  void ProcessHints(const optimization_guide::proto::Configuration& config,
                    const std::string& version) {
    optimization_guide::HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("somefile.pb")));
    ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(config, info.path));

    base::RunLoop run_loop;
    hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager_->OnHintsComponentAvailable(info);
    run_loop.Run();
  }

  void InitializeWithDefaultConfig(const std::string& version) {
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

    ProcessHints(config, version);
  }

  OptimizationGuideHintsManager* hints_manager() const {
    return hints_manager_.get();
  }

  GURL url_with_hints() const {
    return GURL("https://somedomain.org/news/whatever");
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  TestingPrefServiceSimple* pref_service() const { return pref_service_.get(); }

 protected:
  void RunUntilIdle() {
    browser_thread_bundle_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 private:
  void WriteConfigToFile(const optimization_guide::proto::Configuration& config,
                         const base::FilePath& filePath) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_EQ(static_cast<int32_t>(serialized_config.size()),
              base::WriteFile(filePath, serialized_config.data(),
                              serialized_config.size()));
  }

  content::TestBrowserThreadBundle browser_thread_bundle_;

  std::unique_ptr<OptimizationGuideHintsManager> hints_manager_;
  std::unique_ptr<TestOptimizationGuideService> optimization_guide_service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideHintsManagerTest);
};

TEST_F(OptimizationGuideHintsManagerTest,
       ProcessHintsWithValidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, encoded_config);
  CreateServiceAndHintsManager();

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult", 0);
  // However, we still expect the local histogram for the hints being updated to
  // be recorded.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.UpdateComponentHints.Result", true, 1);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ProcessHintsWithInvalidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, "this-is-not-a-proto");
  CreateServiceAndHintsManager();

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult", 0);
  // We also do not expect to update the component hints with bad hints either.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.UpdateComponentHints.Result", 0);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ProcessHintsWithCommandLineOverrideShouldNotBeOverriddenByNewComponent) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  {
    base::HistogramTester histogram_tester;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        optimization_guide::switches::kHintsProtoOverride, encoded_config);
    CreateServiceAndHintsManager();
    // The below histogram should not be recorded since hints weren't coming
    // directly from the component.
    histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult",
                                      0);
    // However, we still expect the local histogram for the hints being updated
    // to be recorded.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.UpdateComponentHints.Result", true, 1);
  }

  // Test that a new component coming in does not update the component hints.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0.0");
    // The below histograms should not be recorded since component hints
    // processing is disabled.
    histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult",
                                      0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.UpdateComponentHints.Result", 0);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ParseTwoConfigVersions) {
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
    InitializeWithDefaultConfig("1.0.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }

  // Test the second time parsing the config. This should also update the hints.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ParseOlderConfigVersions) {
  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("10.0.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as an older version.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0.0");
    // If we have already parsed a version later than this version, we expect
    // for the hints to not be updated.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kSkippedProcessingHints,
        1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ParseDuplicateConfigVersions) {
  const std::string version = "3.0.0.0";

  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig(version);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as a duplicate version.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig(version);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kSkippedProcessingHints,
        1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ComponentInfoDidNotContainConfig) {
  base::HistogramTester histogram_tester;
  ProcessInvalidHintsComponentInfo("1.0.0.0");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      optimization_guide::ProcessHintsComponentResult::kFailedReadingFile, 1);
}

TEST_F(OptimizationGuideHintsManagerTest, ProcessHintsWithExistingPref) {
  // Write hints processing pref for version 2.0.0.
  pref_service()->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion, "2.0.0");

  // Verify config not processed for same version (2.0.0) and pref not cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kFailedFinishProcessing,
        1);
    EXPECT_FALSE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
  }

  // Now verify config is processed for different version and pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0");
    EXPECT_TRUE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ProcessHintsWithInvalidPref) {
  // Create pref file with invalid version.
  pref_service()->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion, "bad-2.0.0");

  // Verify config not processed for existing pref with bad value but
  // that the pref is cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    EXPECT_TRUE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kFailedFinishProcessing,
        1);
  }

  // Now verify config is processed with pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    EXPECT_TRUE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, LoadHintForNavigationWithHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(url_with_hints());

  base::RunLoop run_loop;
  hints_manager()->LoadHintForNavigation(&navigation_handle,
                                         run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
}

TEST_F(OptimizationGuideHintsManagerTest, LoadHintForNavigationNoHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://notinhints.com"));

  base::RunLoop run_loop;
  hints_manager()->LoadHintForNavigation(&navigation_handle,
                                         run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
}

TEST_F(OptimizationGuideHintsManagerTest, LoadHintForNavigationNoHost) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("blargh"));

  base::RunLoop run_loop;
  hints_manager()->LoadHintForNavigation(&navigation_handle,
                                         run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);
}

TEST_F(OptimizationGuideHintsManagerTest,
       OptimizationFiltersAreOnlyLoadedIfTypeIsRegistered) {
  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::NOSCRIPT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);

  {
    base::HistogramTester histogram_tester;

    ProcessHints(config, "1.0.0.0");

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
  }

  // Now register the optimization type and see that it is loaded.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::LITE_PAGE_REDIRECT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::
            kFoundServerBlacklistConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist,
        1);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::LITE_PAGE_REDIRECT));
    EXPECT_FALSE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::NOSCRIPT));
  }

  // Re-registering the same optimization type does not re-load the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::LITE_PAGE_REDIRECT});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
  }

  // Registering a new optimization type without a filter does not trigger a
  // reload of the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::DEFER_ALL_SCRIPT});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
  }

  // Registering a new optimization type with a filter does trigger a
  // reload of the filters.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::NOSCRIPT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::
            kFoundServerBlacklistConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        optimization_guide::OptimizationFilterStatus::
            kFoundServerBlacklistConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist,
        1);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::LITE_PAGE_REDIRECT));
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::NOSCRIPT));
  }
}

TEST_F(OptimizationGuideHintsManagerTest,
       OptimizationFiltersOnlyLoadOncePerType) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // We found 2 LPR blacklists: parsed one and duped the other.
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFoundServerBlacklistConfig,
      2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::
          kFailedServerBlacklistDuplicateConfig,
      1);
}

TEST_F(OptimizationGuideHintsManagerTest, InvalidOptimizationFilterNotLoaded) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  int too_many_bits =
      optimization_guide::features::MaxServerBloomFilterByteSize() * 8 + 1;

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions, too_many_bits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blacklist_bloom_filter,
      kBlackBlacklistBloomFilterNumHashFunctions, too_many_bits, &config);
  ProcessHints(config, "1.0.0.0");

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFoundServerBlacklistConfig,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::
          kFailedServerBlacklistTooBig,
      1);
  EXPECT_FALSE(hints_manager()->HasLoadedOptimizationFilter(
      optimization_guide::proto::LITE_PAGE_REDIRECT));
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetchNotAllowedIfFeatureIsEnabledButTopHostProviderIsNotProvided) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching}, {});

  base::HistogramTester histogram_tester;

  CreateServiceAndHintsManager(/*top_host_provider=*/nullptr);

  histogram_tester.ExpectUniqueSample("OptimizationGuide.HintsFetching.Allowed",
                                      false, 1);
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetchNotAllowedIfFeatureIsNotEnabledButTopHostProviderIsProvided) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kFetchHintsOverride, "whatever.com");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {optimization_guide::features::kOptimizationHintsFetching});

  base::HistogramTester histogram_tester;

  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider =
      optimization_guide::CommandLineTopHostProvider::CreateIfEnabled();
  CreateServiceAndHintsManager(top_host_provider.get());

  histogram_tester.ExpectUniqueSample("OptimizationGuide.HintsFetching.Allowed",
                                      false, 1);
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetchAllowedIfFeatureIsEnabledAndTopHostProviderIsProvided) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kFetchHintsOverride, "whatever.com");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching}, {});

  base::HistogramTester histogram_tester;

  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider =
      optimization_guide::CommandLineTopHostProvider::CreateIfEnabled();
  CreateServiceAndHintsManager(top_host_provider.get());

  histogram_tester.ExpectUniqueSample("OptimizationGuide.HintsFetching.Allowed",
                                      true, 1);
}
