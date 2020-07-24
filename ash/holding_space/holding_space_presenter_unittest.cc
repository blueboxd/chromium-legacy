// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/holding_space/holding_space_presenter.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class HoldingSpacePresenterTest : public AshTestBase {
 public:
  HoldingSpacePresenterTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kTemporaryHoldingSpace);
  }
  HoldingSpacePresenterTest(const HoldingSpacePresenterTest& other) = delete;
  HoldingSpacePresenterTest& operator=(const HoldingSpacePresenterTest& other) =
      delete;
  ~HoldingSpacePresenterTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    holding_space_presenter_ = std::make_unique<HoldingSpacePresenter>();
  }

  void TearDown() override {
    holding_space_presenter_.reset();
    AshTestBase::TearDown();
  }

  HoldingSpaceModel* primary_model() { return &primary_model_; }

  HoldingSpacePresenter* GetHoldingSpacePresenter() {
    return holding_space_presenter_.get();
  }

 private:
  std::unique_ptr<HoldingSpacePresenter> holding_space_presenter_;
  HoldingSpaceModel primary_model_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that items already in the model when an active model is set get added
// to the holding space.
TEST_F(HoldingSpacePresenterTest, ModelWithExistingItems) {
  EXPECT_EQ(std::vector<std::string>(), GetHoldingSpacePresenter()->item_ids());

  const std::string item_1_id = "item_1";
  primary_model()->AddItem(std::make_unique<HoldingSpaceItem>(item_1_id));
  ASSERT_TRUE(primary_model()->GetItem(item_1_id));
  ASSERT_EQ(item_1_id, primary_model()->GetItem(item_1_id)->id());

  const std::string item_2_id = "item_2";
  primary_model()->AddItem(std::make_unique<HoldingSpaceItem>(item_2_id));
  ASSERT_TRUE(primary_model()->GetItem(item_2_id));
  ASSERT_EQ(item_2_id, primary_model()->GetItem(item_2_id)->id());

  // Note - the item ID is missing a suffix to verify the items do not get
  // sorted by their IDs.
  const std::string item_3_id = "item";
  primary_model()->AddItem(std::make_unique<HoldingSpaceItem>(item_3_id));

  HoldingSpaceController::Get()->SetModel(primary_model());
  EXPECT_EQ(std::vector<std::string>({item_1_id, item_2_id, item_3_id}),
            GetHoldingSpacePresenter()->item_ids());

  HoldingSpaceController::Get()->SetModel(nullptr);
  EXPECT_EQ(std::vector<std::string>(), GetHoldingSpacePresenter()->item_ids());
}

// Tests that the holding space presenter picks up existing model items if a
// model is set and non-empty on presenter's creation.
TEST_F(HoldingSpacePresenterTest, NonEmptyModelOnPresenterCreation) {
  // Initiate non-empty holding space model.
  const std::string item_1_id = "item_1";
  primary_model()->AddItem(std::make_unique<HoldingSpaceItem>(item_1_id));
  const std::string item_2_id = "item_2";
  primary_model()->AddItem(std::make_unique<HoldingSpaceItem>(item_2_id));
  HoldingSpaceController::Get()->SetModel(primary_model());

  // Create a new holding space presenter, and verify it picked up the existing
  // model items.
  auto secondary_presenter = std::make_unique<HoldingSpacePresenter>();
  EXPECT_EQ(std::vector<std::string>({item_1_id, item_2_id}),
            secondary_presenter->item_ids());

  HoldingSpaceController::Get()->SetModel(nullptr);
  EXPECT_EQ(std::vector<std::string>(), GetHoldingSpacePresenter()->item_ids());
  EXPECT_EQ(std::vector<std::string>(), secondary_presenter->item_ids());
}

// Verifies that holding space handles holding space model changes.
TEST_F(HoldingSpacePresenterTest, AddingAndRemovingModelItems) {
  HoldingSpaceController::Get()->SetModel(primary_model());
  EXPECT_EQ(std::vector<std::string>(), GetHoldingSpacePresenter()->item_ids());

  // Add some items to the model, and verify the items get picked up by the
  // presenter.
  const std::string item_1_id = "item_1";
  primary_model()->AddItem(std::make_unique<HoldingSpaceItem>(item_1_id));
  EXPECT_EQ(std::vector<std::string>({item_1_id}),
            GetHoldingSpacePresenter()->item_ids());

  const std::string item_2_id = "item_2";
  primary_model()->AddItem(std::make_unique<HoldingSpaceItem>(item_2_id));
  EXPECT_EQ(std::vector<std::string>({item_1_id, item_2_id}),
            GetHoldingSpacePresenter()->item_ids());

  const std::string item_3_id = "item_3";
  primary_model()->AddItem(std::make_unique<HoldingSpaceItem>(item_3_id));
  EXPECT_EQ(std::vector<std::string>({item_1_id, item_2_id, item_3_id}),
            GetHoldingSpacePresenter()->item_ids());

  primary_model()->RemoveItem(item_2_id);
  EXPECT_EQ(std::vector<std::string>({item_1_id, item_3_id}),
            GetHoldingSpacePresenter()->item_ids());

  primary_model()->RemoveAll();
  EXPECT_EQ(std::vector<std::string>(), GetHoldingSpacePresenter()->item_ids());

  const std::string item_4_id = "item_4";
  primary_model()->AddItem(std::make_unique<HoldingSpaceItem>(item_4_id));
  EXPECT_EQ(std::vector<std::string>({item_4_id}),
            GetHoldingSpacePresenter()->item_ids());

  // Holding space should be cleared if the active model is reset.
  HoldingSpaceController::Get()->SetModel(nullptr);
  EXPECT_EQ(std::vector<std::string>(), GetHoldingSpacePresenter()->item_ids());
}

// Verifies that the holding space gets updated when the active model changes.
TEST_F(HoldingSpacePresenterTest, ModelChange) {
  const std::string primary_model_item_id = "primary_model_item";
  primary_model()->AddItem(
      std::make_unique<HoldingSpaceItem>(primary_model_item_id));

  HoldingSpaceController::Get()->SetModel(primary_model());
  EXPECT_EQ(std::vector<std::string>({primary_model_item_id}),
            GetHoldingSpacePresenter()->item_ids());

  HoldingSpaceModel secondary_model;
  const std::string secondary_model_item_id = "secondary_model_item";
  secondary_model.AddItem(
      std::make_unique<HoldingSpaceItem>(secondary_model_item_id));

  EXPECT_EQ(std::vector<std::string>({primary_model_item_id}),
            GetHoldingSpacePresenter()->item_ids());

  HoldingSpaceController::Get()->SetModel(&secondary_model);

  EXPECT_EQ(std::vector<std::string>({secondary_model_item_id}),
            GetHoldingSpacePresenter()->item_ids());

  HoldingSpaceController::Get()->SetModel(nullptr);
  EXPECT_EQ(std::vector<std::string>(), GetHoldingSpacePresenter()->item_ids());
}

}  // namespace ash
