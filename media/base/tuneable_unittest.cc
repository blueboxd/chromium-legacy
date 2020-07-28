// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/tuneable.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class TuneableTest : public ::testing::Test {
 public:
  TuneableTest() = default;

  void SetUp() override {
    // Note that we might need to call value() to cache `tuneable_cached_` here.
    // We don't currently, since it's not needed.

    // Set everything else to non-default values.  We do this because we can't
    // do it in the tests without it doing odd things.
    // params[kTuneableIntSetToNot123] = "124";  // Not 123.
    // params[kTuneableInt0] = "0";
    // params[kTuneableInt5] = "5";
    // params[kTuneableInt10] = "10";

    // Set some tuneables to fixed values.
    SetFinchParameters(kTuneableIntSetToNot123, 124, 124);
    SetFinchParameters(kTuneableInt0, 0, 0);
    SetFinchParameters(kTuneableInt5, 5, 5);
    SetFinchParameters(kTuneableInt10, 10, 10);
    // TimeDelta should be given in milliseconds.
    SetFinchParameters(kTuneableTimeDeltaFiveSeconds, 5000, 5000);

    // Let some vary via finch trial.
    SetFinchParameters(kTuneableInt5To10, 5, 10);
    // Create 100 identical tuneables, except for their names.
    for (int i = 0; i < 100; i++) {
      SetFinchParameters(GetNameForNumberedTuneable(k100Tuneables, i).c_str(),
                         1, 100);
    }

    scoped_feature_list_.InitAndEnableFeatureWithParameters(kMediaOptimizer,
                                                            params_);
  }

  // Set the finch-chosen parameters for tuneable `name`.
  void SetFinchParameters(const char* name, int min_value, int max_value) {
    std::string min_name = std::string(name) + "_min";
    params_[min_name] = base::NumberToString(min_value);
    std::string max_name = std::string(name) + "_max";
    params_[max_name] = base::NumberToString(max_value);
  }

  // Return the tuneable name for the `x`-th numbered tuneable.
  std::string GetNameForNumberedTuneable(const char* basename, int x) {
    std::string name(basename);
    name[0] = "0123456789ABCDEF"[x & 15];
    name[1] = "0123456789ABCDEF"[(x >> 4) & 15];
    return name;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::FieldTrialParams params_;

  // Params will set this to not 123.  We default it to 123 before Setup runs,
  // so we make sure that it uses the hardcoded defaults properly.
  static constexpr const char* kTuneableIntSetToNot123 = "t_int_not_123";
  Tuneable<int> tuneable_cached_{kTuneableIntSetToNot123, 0, 123, 200};

  static constexpr const char* kTuneableIntUnset = "t_int_unset";
  static constexpr const char* kTuneableInt0 = "t_int_0";
  static constexpr const char* kTuneableInt5 = "t_int_5";
  static constexpr const char* kTuneableInt10 = "t_int_10";
  static constexpr const char* kTuneableTimeDeltaFiveSeconds = "t_time_5s";

  static constexpr const char* kTuneableInt5To10 = "t_int_5to10";

  // Initialize 100 of these with different names.
  static constexpr const char* k100Tuneables = "XX_tuneable";

  DISALLOW_COPY_AND_ASSIGN(TuneableTest);
};

TEST_F(TuneableTest, IntTuneableCached) {
  // Verify that `tuneable_cached_` is, in fact, 123 even though the params try
  // to set it to something else.  This kind of, sort of, guarantees that it's
  // cached properly.
  EXPECT_EQ(tuneable_cached_.value(), 123);
}

TEST_F(TuneableTest, IntTuneableFromDefaultWithClamps) {
  // The default value should be used, and correctly clamped.
  constexpr int min_value = 0;
  constexpr int default_value = 4;
  constexpr int max_value = 10;
  Tuneable<int> t_min(kTuneableIntUnset, min_value, min_value - 1, max_value);
  EXPECT_EQ(t_min.value(), min_value);
  Tuneable<int> t_default(kTuneableIntUnset, min_value, default_value,
                          max_value);
  EXPECT_EQ(t_default.value(), default_value);
  Tuneable<int> t_max(kTuneableIntUnset, min_value, max_value + 1, max_value);
  EXPECT_EQ(t_max.value(), max_value);
}

TEST_F(TuneableTest, IntTuneableFromParams) {
  // Verify that params override the defaults, and are clamped correctly.
  constexpr int min_value = 1;
  constexpr int default_value = 4;  // That's not the same as the param.
  constexpr int max_value = 9;
  Tuneable<int> t_min(kTuneableInt0, min_value, default_value, max_value);
  EXPECT_EQ(t_min.value(), min_value);
  Tuneable<int> t_param(kTuneableInt5, min_value, default_value, max_value);
  EXPECT_EQ(t_param.value(), 5);
  Tuneable<int> t_max(kTuneableInt10, min_value, default_value, max_value);
  EXPECT_EQ(t_max.value(), max_value);
}

TEST_F(TuneableTest, OtherSpecializationsCompile) {
  // Since it's all templated, just be happy if it compiles and does something
  // somewhat sane.
  constexpr base::TimeDelta min_value = base::TimeDelta::FromSeconds(0);
  constexpr base::TimeDelta default_value = base::TimeDelta::FromSeconds(5);
  constexpr base::TimeDelta max_value = base::TimeDelta::FromSeconds(10);
  Tuneable<base::TimeDelta> time_delta_tuneable("whatever", min_value,
                                                default_value, max_value);
  // Since the tuneable is not provided in the finch parameters, it should
  // equal the default.
  EXPECT_EQ(time_delta_tuneable.value(), default_value);

  Tuneable<size_t> size_t_tuneable("whatever_else", 0u, 100u, 500u);
  EXPECT_EQ(size_t_tuneable.value(), 100u);
}

TEST_F(TuneableTest, TimeDeltaIsSpecifiedInMilliseconds) {
  // Since the finch params are constructed with the assumption that the value
  // will be interpreted as milliseconds, make sure that the Tuneable actually
  // does interpret it that way.
  constexpr base::TimeDelta min_value = base::TimeDelta::FromSeconds(0);
  constexpr base::TimeDelta max_value = base::TimeDelta::FromSeconds(100);
  Tuneable<base::TimeDelta> t(kTuneableTimeDeltaFiveSeconds, min_value,
                              min_value, max_value);
  EXPECT_EQ(t.value(), base::TimeDelta::FromSeconds(5));
}

TEST_F(TuneableTest, MultipleTuneablesGetTheSameRandomValue) {
  // Multiple copies of the same tuneable should get the same value.
  Tuneable<int> t0(kTuneableInt5To10, 0, 2, 100);
  Tuneable<int> t1(kTuneableInt5To10, 0, 2, 100);
  EXPECT_GE(t0.value(), 0);
  EXPECT_LE(t0.value(), 100);
  EXPECT_EQ(t0.value(), t1.value());
}

TEST_F(TuneableTest, DifferentSeedsProduceDifferentValues) {
  // Also verify that they stay bounded.
  SetRandomSeedForTuneables(base::UnguessableToken::Create());
  Tuneable<int> t0(kTuneableInt5To10, 0, 2, 100);
  bool found_different = false;
  for (int i = 1; i < 100; i++) {
    SetRandomSeedForTuneables(base::UnguessableToken::Create());
    Tuneable<int> t1(kTuneableInt5To10, 0, 2, 100);
    EXPECT_GE(t1.value(), 0);
    EXPECT_LE(t1.value(), 100);
    if (t1.value() != t0.value())
      found_different = true;
  }
  EXPECT_TRUE(found_different);
}

TEST_F(TuneableTest, DifferentNamesProduceDifferentValues) {
  // For the same seed, we expect different parameter names to sometimes get
  // different values.
  Tuneable<int> t0(GetNameForNumberedTuneable(k100Tuneables, 0).c_str(), 0, 50,
                   100);
  bool found_different = false;
  for (int i = 1; i < 100; i++) {
    Tuneable<int> t1(GetNameForNumberedTuneable(k100Tuneables, i).c_str(), 0,
                     50, 100);
    EXPECT_GE(t1.value(), 0);
    EXPECT_LE(t1.value(), 100);
    if (t1.value() != t0.value())
      found_different = true;
  }
  EXPECT_TRUE(found_different);
}

}  // namespace media
