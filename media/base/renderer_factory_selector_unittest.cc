// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "media/base/overlay_info.h"
#include "media/base/renderer_factory_selector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class RendererFactorySelectorTest : public testing::Test {
 public:
  using FactoryType = RendererFactoryType;

  class FakeFactory : public RendererFactory {
   public:
    explicit FakeFactory(FactoryType type) : type_(type) {}

    std::unique_ptr<Renderer> CreateRenderer(
        const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
        const scoped_refptr<base::TaskRunner>& worker_task_runner,
        AudioRendererSink* audio_renderer_sink,
        VideoRendererSink* video_renderer_sink,
        RequestOverlayInfoCB request_overlay_info_cb,
        const gfx::ColorSpace& target_color_space) override {
      return nullptr;
    }

    FactoryType factory_type() { return type_; }

   private:
    FactoryType type_;
  };

  RendererFactorySelectorTest() = default;

  void AddBaseFactory(FactoryType type) {
    selector_.AddBaseFactory(type, std::make_unique<FakeFactory>(type));
  }

  void AddFactory(FactoryType type) {
    selector_.AddFactory(type, std::make_unique<FakeFactory>(type));
  }

  void AddConditionalFactory(FactoryType type) {
    condition_met_map_[type] = false;
    selector_.AddConditionalFactory(
        type, std::make_unique<FakeFactory>(type),
        base::BindRepeating(&RendererFactorySelectorTest::IsConditionMet,
                            base::Unretained(this), type));
  }

  FactoryType GetCurrentlySelectedFactoryType() {
    return reinterpret_cast<FakeFactory*>(selector_.GetCurrentFactory())
        ->factory_type();
  }

  bool IsConditionMet(FactoryType type) {
    DCHECK(condition_met_map_.count(type));
    return condition_met_map_[type];
  }

 protected:
  RendererFactorySelector selector_;
  std::map<FactoryType, bool> condition_met_map_;

  DISALLOW_COPY_AND_ASSIGN(RendererFactorySelectorTest);
};

TEST_F(RendererFactorySelectorTest, SingleFactory) {
  AddBaseFactory(FactoryType::kDefault);
  EXPECT_EQ(FactoryType::kDefault, GetCurrentlySelectedFactoryType());
}

TEST_F(RendererFactorySelectorTest, MultipleFactory) {
  AddBaseFactory(FactoryType::kDefault);
  AddFactory(FactoryType::kMojo);

  EXPECT_EQ(FactoryType::kDefault, GetCurrentlySelectedFactoryType());

  selector_.SetBaseFactoryType(FactoryType::kMojo);
  EXPECT_EQ(FactoryType::kMojo, GetCurrentlySelectedFactoryType());
}

TEST_F(RendererFactorySelectorTest, ConditionalFactory) {
  AddBaseFactory(FactoryType::kDefault);
  AddFactory(FactoryType::kMojo);
  AddConditionalFactory(FactoryType::kCourier);

  EXPECT_EQ(FactoryType::kDefault, GetCurrentlySelectedFactoryType());

  condition_met_map_[FactoryType::kCourier] = true;
  EXPECT_EQ(FactoryType::kCourier, GetCurrentlySelectedFactoryType());

  selector_.SetBaseFactoryType(FactoryType::kMojo);
  EXPECT_EQ(FactoryType::kCourier, GetCurrentlySelectedFactoryType());

  condition_met_map_[FactoryType::kCourier] = false;
  EXPECT_EQ(FactoryType::kMojo, GetCurrentlySelectedFactoryType());
}

TEST_F(RendererFactorySelectorTest, MultipleConditionalFactories) {
  AddBaseFactory(FactoryType::kDefault);
  AddConditionalFactory(FactoryType::kFlinging);
  AddConditionalFactory(FactoryType::kCourier);

  EXPECT_EQ(FactoryType::kDefault, GetCurrentlySelectedFactoryType());

  condition_met_map_[FactoryType::kFlinging] = false;
  condition_met_map_[FactoryType::kCourier] = true;
  EXPECT_EQ(FactoryType::kCourier, GetCurrentlySelectedFactoryType());

  condition_met_map_[FactoryType::kFlinging] = true;
  condition_met_map_[FactoryType::kCourier] = false;
  EXPECT_EQ(FactoryType::kFlinging, GetCurrentlySelectedFactoryType());

  // It's up to the implementation detail to decide which one to use.
  condition_met_map_[FactoryType::kFlinging] = true;
  condition_met_map_[FactoryType::kCourier] = true;
  EXPECT_TRUE(GetCurrentlySelectedFactoryType() == FactoryType::kFlinging ||
              GetCurrentlySelectedFactoryType() == FactoryType::kCourier);
}

}  // namespace media
