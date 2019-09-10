// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_json_parser.h"

#include <stddef.h>

#include "cc/layers/layer.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

bool LayerTreesMatch(LayerImpl* const layer_impl,
                     Layer* const layer) {
#define RETURN_IF_EXPECTATION_FAILS(exp) \
  do { \
    exp; \
    if (testing::UnitTest::GetInstance()->current_test_info()-> \
            result()->Failed()) \
      return false; \
  } while (0)

  RETURN_IF_EXPECTATION_FAILS(
      EXPECT_EQ(layer_impl->test_properties()->children.size(),
                layer->children().size()));
  RETURN_IF_EXPECTATION_FAILS(EXPECT_EQ(layer_impl->bounds(), layer->bounds()));
  RETURN_IF_EXPECTATION_FAILS(EXPECT_TRANSFORMATION_MATRIX_EQ(
      layer_impl->test_properties()->transform, layer->transform()));
  RETURN_IF_EXPECTATION_FAILS(EXPECT_EQ(layer_impl->contents_opaque(),
                                        layer->contents_opaque()));
  RETURN_IF_EXPECTATION_FAILS(EXPECT_EQ(layer_impl->scrollable(),
                                        layer->scrollable()));

  RETURN_IF_EXPECTATION_FAILS(
      EXPECT_FLOAT_EQ(layer_impl->Opacity(), layer->opacity()));
  RETURN_IF_EXPECTATION_FAILS(EXPECT_EQ(layer_impl->touch_action_region(),
                                        layer->touch_action_region()));

  for (size_t i = 0; i < layer_impl->test_properties()->children.size(); ++i) {
    RETURN_IF_EXPECTATION_FAILS(
        EXPECT_TRUE(LayerTreesMatch(layer_impl->test_properties()->children[i],
                                    layer->children()[i].get())));
  }

  return true;
#undef RETURN_IF_EXPECTATION_FAILS
}

}  // namespace

class LayerTreeJsonParserSanityCheck : public LayerTestCommon::LayerImplTest,
                                       public testing::Test {};

TEST_F(LayerTreeJsonParserSanityCheck, Basic) {
  LayerTreeImpl* tree = host_impl()->active_tree();

  auto* root = root_layer();
  auto* parent = AddLayer<LayerImpl>();
  auto* child = AddLayer<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  parent->SetBounds(gfx::Size(50, 50));
  child->SetBounds(gfx::Size(40, 40));

  CopyProperties(root, parent);
  parent->SetOffsetToTransformParent(gfx::Vector2dF(25.f, 25.f));

  gfx::Transform translate;
  translate.Translate(10, 15);
  CopyProperties(parent, child);
  CreateTransformNode(child).local = translate;

  UpdateDrawProperties(tree);

  std::string json = tree->LayerTreeAsJson();
  scoped_refptr<Layer> new_root = ParseTreeFromJson(json, nullptr);
  ASSERT_TRUE(new_root.get());
  EXPECT_TRUE(LayerTreesMatch(root, new_root.get()));
}

TEST_F(LayerTreeJsonParserSanityCheck, EventHandlerRegions) {
  LayerTreeImpl* tree = host_impl()->active_tree();

  auto* root = root_layer();
  auto* touch_layer = AddLayer<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  touch_layer->SetBounds(gfx::Size(50, 50));

  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(10, 10, 20, 30));
  touch_action_region.Union(kTouchActionNone, gfx::Rect(40, 10, 20, 20));
  touch_layer->SetTouchActionRegion(std::move(touch_action_region));

  CopyProperties(root, touch_layer);
  UpdateDrawProperties(tree);

  std::string json = tree->LayerTreeAsJson();
  scoped_refptr<Layer> new_root = ParseTreeFromJson(json, nullptr);
  ASSERT_TRUE(new_root.get());
  EXPECT_TRUE(LayerTreesMatch(root, new_root.get()));
}

}  // namespace cc
