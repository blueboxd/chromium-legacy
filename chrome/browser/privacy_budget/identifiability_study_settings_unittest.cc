// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/identifiability_study_settings.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/browser/privacy_budget/scoped_privacy_budget_config.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace test_utils {

// This class is a friend of IdentifiabilityStudySettings and can reach into the
// internals. Use this as a last resort.
class InspectableIdentifiabilityStudySettings
    : public IdentifiabilityStudySettings {
 public:
  using IdentifiabilityStudySettings::IdentifiableSurfaceSet;
  using IdentifiabilityStudySettings::IdentifiableSurfaceTypeSet;

  explicit InspectableIdentifiabilityStudySettings(PrefService* pref_service)
      : IdentifiabilityStudySettings(pref_service) {}

  const IdentifiableSurfaceSet& active_surfaces() const {
    return active_surfaces_;
  }
  const IdentifiableSurfaceSet& retired_surfaces() const {
    return retired_surfaces_;
  }
  int max_active_surfaces() const { return max_active_surfaces_; }
  int surface_selection_rate() const { return surface_selection_rate_; }
  uint64_t prng_seed() const { return prng_seed_; }
};

}  // namespace test_utils

namespace {

// Constants used to set up the test configuration.
constexpr int kTestingGeneration = 58;
constexpr auto kBlockedSurface1 = blink::IdentifiableSurface::FromMetricHash(1);
constexpr auto kFakeSeed = UINT64_C(9);
constexpr auto kBlockedType1 =
    blink::IdentifiableSurface::Type::kCanvasReadback;

// Sample surfaces.
constexpr auto kRegularSurface1 =
    blink::IdentifiableSurface::FromMetricHash(256 + 3);
constexpr auto kRegularSurface2 =
    blink::IdentifiableSurface::FromMetricHash(256 + 4);
constexpr auto kRegularSurface3 =
    blink::IdentifiableSurface::FromMetricHash(256 + 5);
constexpr auto kBlockedTypeSurface1 =
    blink::IdentifiableSurface::FromTypeAndInput(
        blink::IdentifiableSurface::Type::kCanvasReadback,
        1);  // = 258

std::string SurfaceListString(
    std::initializer_list<blink::IdentifiableSurface> list) {
  std::vector<std::string> list_as_strings(list.size());
  std::transform(
      list.begin(), list.end(), list_as_strings.begin(),
      [](const auto& v) { return base::NumberToString(v.ToUkmMetricHash()); });
  return base::JoinString(list_as_strings, ",");
}

// Make names short
using IdentifiableSurfaceSet =
    test_utils::InspectableIdentifiabilityStudySettings::IdentifiableSurfaceSet;
using IdentifiableSurfaceTypeSet = test_utils::
    InspectableIdentifiabilityStudySettings::IdentifiableSurfaceTypeSet;

}  // namespace

class IdentifiabilityStudySettingsTest : public ::testing::Test {
 public:
  IdentifiabilityStudySettingsTest() {
    // Uses FeatureLists. Hence needs to be initialized in the constructor lest
    // we add any multithreading tests here.
    auto parameters = test::ScopedPrivacyBudgetConfig::Parameters{};
    parameters.generation = kTestingGeneration;
    parameters.blocked_surfaces.push_back(kBlockedSurface1);
    parameters.blocked_types.push_back(kBlockedType1);
    config_.Apply(parameters);
    prefs::RegisterPrivacyBudgetPrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
  test::ScopedPrivacyBudgetConfig config_;
};

TEST_F(IdentifiabilityStudySettingsTest, InstantiateAndInitialize) {
  auto settings =
      std::make_unique<IdentifiabilityStudySettings>(pref_service());

  // Successful initialization should result in a new PRNG seed and setting the
  // generation number.
  EXPECT_EQ(kTestingGeneration,
            pref_service()->GetInteger(prefs::kPrivacyBudgetGeneration));
  EXPECT_NE(UINT64_C(0), pref_service()->GetUint64(prefs::kPrivacyBudgetSeed));
}

TEST_F(IdentifiabilityStudySettingsTest, ReInitializeWhenGenerationChanges) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration - 1);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);

  auto settings =
      std::make_unique<IdentifiabilityStudySettings>(pref_service());

  // Successful re-initialization should result in a new PRNG seed and setting
  // the generation number.
  EXPECT_EQ(kTestingGeneration,
            pref_service()->GetInteger(prefs::kPrivacyBudgetGeneration));
  EXPECT_NE(kFakeSeed, pref_service()->GetUint64(prefs::kPrivacyBudgetSeed));
}

TEST_F(IdentifiabilityStudySettingsTest, LoadsFromPrefs) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);
  pref_service()->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      SurfaceListString({kRegularSurface1, kRegularSurface2}));
  pref_service()->SetString(prefs::kPrivacyBudgetRetiredSurfaces,
                            SurfaceListString({kBlockedTypeSurface1}));
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudySettings>(
          pref_service());
  EXPECT_EQ((IdentifiableSurfaceSet{kRegularSurface1, kRegularSurface2}),
            settings->active_surfaces());
  EXPECT_EQ((IdentifiableSurfaceSet{kBlockedTypeSurface1}),
            settings->retired_surfaces());
}

TEST_F(IdentifiabilityStudySettingsTest, ReconcileBlockedSurfaces) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);
  pref_service()->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      SurfaceListString(
          {kBlockedSurface1, kRegularSurface1, kRegularSurface2}));
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudySettings>(
          pref_service());
  EXPECT_EQ((IdentifiableSurfaceSet{kRegularSurface1, kRegularSurface2}),
            settings->active_surfaces());
  EXPECT_EQ((IdentifiableSurfaceSet{kBlockedSurface1}),
            settings->retired_surfaces());
}

TEST_F(IdentifiabilityStudySettingsTest, ReconcileBlockedTypes) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);
  pref_service()->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      SurfaceListString(
          {kBlockedTypeSurface1, kRegularSurface1, kRegularSurface2}));
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudySettings>(
          pref_service());
  EXPECT_EQ((IdentifiableSurfaceSet{kRegularSurface1, kRegularSurface2}),
            settings->active_surfaces());
  EXPECT_EQ((IdentifiableSurfaceSet{kBlockedTypeSurface1}),
            settings->retired_surfaces());
  EXPECT_EQ("258",
            pref_service()->GetString(prefs::kPrivacyBudgetRetiredSurfaces));
}

TEST_F(IdentifiabilityStudySettingsTest, AllowsActive) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);
  pref_service()->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      SurfaceListString(
          {kRegularSurface1, kRegularSurface2, kRegularSurface3}));
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudySettings>(
          pref_service());
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface1));
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface2));
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface3));
  EXPECT_EQ((IdentifiableSurfaceSet{kRegularSurface1, kRegularSurface2,
                                    kRegularSurface3}),
            settings->active_surfaces());
}

TEST_F(IdentifiabilityStudySettingsTest, BlocksBlocked) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingGeneration);
  pref_service()->SetUint64(prefs::kPrivacyBudgetSeed, kFakeSeed);
  pref_service()->SetString(
      prefs::kPrivacyBudgetActiveSurfaces,
      SurfaceListString({kRegularSurface1, kRegularSurface2}));
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudySettings>(
          pref_service());
  EXPECT_FALSE(settings->ShouldRecordSurface(kBlockedSurface1));
  EXPECT_FALSE(settings->ShouldRecordSurface(kBlockedTypeSurface1));
}

TEST_F(IdentifiabilityStudySettingsTest, UpdatesActive) {
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudySettings>(
          pref_service());
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface1));
  EXPECT_EQ((IdentifiableSurfaceSet{kRegularSurface1}),
            settings->active_surfaces());
  EXPECT_EQ(SurfaceListString({kRegularSurface1}),
            pref_service()->GetString(prefs::kPrivacyBudgetActiveSurfaces));
}

// Verify that the study parameters don't overflow.
TEST(IdentifiabilityStudySettingsStandaloneTest, HighClamps) {
  auto params = test::ScopedPrivacyBudgetConfig::Parameters{};
  params.max_surfaces =
      IdentifiabilityStudySettings::kMaxSampledIdentifiableSurfaces + 1;
  params.surface_selection_rate =
      IdentifiabilityStudySettings::kMaxSamplingRateDenominator + 1;
  test::ScopedPrivacyBudgetConfig config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudySettings settings(&pref_service);

  EXPECT_EQ(IdentifiabilityStudySettings::kMaxSampledIdentifiableSurfaces,
            settings.max_active_surfaces());
  EXPECT_EQ(IdentifiabilityStudySettings::kMaxSamplingRateDenominator,
            settings.surface_selection_rate());
}

// Verify that the study parameters don't underflow.
TEST(IdentifiabilityStudySettingsStandaloneTest, LowClamps) {
  auto params = test::ScopedPrivacyBudgetConfig::Parameters{};
  params.max_surfaces = -1;
  params.surface_selection_rate = -1;
  test::ScopedPrivacyBudgetConfig config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudySettings settings(&pref_service);

  EXPECT_EQ(0, settings.max_active_surfaces());
  EXPECT_EQ(0, settings.surface_selection_rate());
}

TEST(IdentifiabilityStudySettingsStandaloneTest, Disabled) {
  auto params = test::ScopedPrivacyBudgetConfig::Parameters{};
  params.enabled = false;
  params.surface_selection_rate = 1;
  test::ScopedPrivacyBudgetConfig config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudySettings settings(&pref_service);

  EXPECT_FALSE(settings.enabled());
  EXPECT_FALSE(settings.ShouldRecordSurface(kRegularSurface1));
  EXPECT_FALSE(settings.ShouldRecordSurface(kRegularSurface2));
  EXPECT_FALSE(settings.ShouldRecordSurface(kRegularSurface3));
}
