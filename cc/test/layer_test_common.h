// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TEST_COMMON_H_
#define CC_TEST_LAYER_TEST_COMMON_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "cc/animation/animation_timeline.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/quads/render_pass.h"

#define EXPECT_SET_NEEDS_COMMIT(expect, code_to_test)                 \
  do {                                                                \
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times((expect)); \
    code_to_test;                                                     \
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());         \
  } while (false)

#define EXPECT_SET_NEEDS_UPDATE(expect, code_to_test)                       \
  do {                                                                      \
    EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times((expect)); \
    code_to_test;                                                           \
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());               \
  } while (false)

namespace gfx { class Rect; }

namespace viz {
class QuadList;
}

namespace cc {
class LayerImpl;
class LayerTreeFrameSink;
class RenderSurfaceImpl;

class LayerListSettings : public LayerTreeSettings {
 public:
  LayerListSettings() { use_layer_lists = true; }
};

class LayerTestCommon {
 public:
  static const char* quad_string;

  static void VerifyQuadsExactlyCoverRect(const viz::QuadList& quads,
                                          const gfx::Rect& rect);

  static void VerifyQuadsAreOccluded(const viz::QuadList& quads,
                                     const gfx::Rect& occluded,
                                     size_t* partially_occluded_count);

  static void SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      LayerTreeHostImpl* host_impl,
      LayerTreeImpl* tree_impl,
      float top_controls_height,
      const gfx::Size& inner_viewport_size,
      const gfx::Size& outer_viewport_size,
      const gfx::Size& scroll_layer_size);

  class LayerImplTest {
   public:
    LayerImplTest();
    explicit LayerImplTest(
        std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink);
    explicit LayerImplTest(const LayerTreeSettings& settings);
    LayerImplTest(const LayerTreeSettings& settings,
                  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink);
    ~LayerImplTest();

    template <typename T, typename... Args>
    T* AddLayer(Args&&... args) {
      return AddLayerInternal<T>(host_impl()->active_tree(),
                                 std::forward<Args>(args)...);
    }

    LayerImpl* EnsureRootLayerInPendingTree();

    template <typename T, typename... Args>
    T* AddLayerInPendingTree(Args&&... args) {
      return AddLayerInternal<T>(host_impl()->pending_tree(),
                                 std::forward<Args>(args)...);
    }

    // TODO(crbug.com/994361): Remove this function when all impl-side tests are
    // converted into layer list mode.
    template <typename T, typename... Args>
    T* AddChildToRoot(Args&&... args) {
      return AddLayer<T>(std::forward<Args>(args)...);
    }

    // TODO(crbug.com/994361): Remove this function when all impl-side tests are
    // converted into layer list mode.
    template <typename T, typename... Args>
    T* AddChild(LayerImpl* parent, Args&&... args) {
      std::unique_ptr<T> layer =
          T::Create(host_impl()->active_tree(), layer_impl_id_++,
                    std::forward<Args>(args)...);
      T* ptr = layer.get();
      parent->test_properties()->AddChild(std::move(layer));
      return ptr;
    }

    template <typename T>
    T* AddMaskLayer(LayerImpl* origin) {
      std::unique_ptr<T> layer =
          T::Create(host_impl()->active_tree(), layer_impl_id_++);
      T* ptr = layer.get();
      origin->test_properties()->SetMaskLayer(std::move(layer));
      return ptr;
    }

    void CalcDrawProps(const gfx::Size& viewport_size);
    void AppendQuadsWithOcclusion(LayerImpl* layer_impl,
                                  const gfx::Rect& occluded);
    void AppendQuadsForPassWithOcclusion(LayerImpl* layer_impl,
                                         viz::RenderPass* given_render_pass,
                                         const gfx::Rect& occluded);
    void AppendSurfaceQuadsWithOcclusion(RenderSurfaceImpl* surface_impl,
                                         const gfx::Rect& occluded);

    void RequestCopyOfOutput();

    LayerTreeFrameSink* layer_tree_frame_sink() const {
      return host_impl()->layer_tree_frame_sink();
    }
    viz::ClientResourceProvider* resource_provider() const {
      return host_impl()->resource_provider();
    }
    LayerImpl* root_layer_for_testing() const {
      return host_impl()->active_tree()->root_layer_for_testing();
    }
    FakeLayerTreeHost* host() { return host_.get(); }
    FakeLayerTreeHostImpl* host_impl() const { return host_->host_impl(); }
    TaskRunnerProvider* task_runner_provider() const {
      return host_impl()->task_runner_provider();
    }
    const viz::QuadList& quad_list() const { return render_pass_->quad_list; }
    scoped_refptr<AnimationTimeline> timeline() { return timeline_; }
    scoped_refptr<AnimationTimeline> timeline_impl() { return timeline_impl_; }

    void BuildPropertyTreesForTesting() {
      host_impl()->active_tree()->BuildPropertyTreesForTesting();
    }

    void SetElementIdsForTesting() {
      host_impl()->active_tree()->SetElementIdsForTesting();
    }

    void ExecuteCalculateDrawProperties(
        LayerImpl* root_layer,
        float device_scale_factor = 1.0f,
        const gfx::Transform& device_transform = gfx::Transform(),
        float page_scale_factor = 1.0f,
        LayerImpl* page_scale_layer = nullptr);

    void ExecuteCalculateDrawPropertiesWithoutAdjustingRasterScales(
        LayerImpl* root_layer);

    const RenderSurfaceList* render_surface_list_impl() const {
      return render_surface_list_impl_.get();
    }

    bool UpdateLayerImplListContains(int id) const {
      for (const auto* layer : update_layer_impl_list_) {
        if (layer->id() == id)
          return true;
      }
      return false;
    }

    const LayerImplList& update_layer_impl_list() const {
      return update_layer_impl_list_;
    }

   private:
    template <typename T, typename... Args>
    T* AddLayerInternal(LayerTreeImpl* tree, Args&&... args) {
      std::unique_ptr<T> layer =
          T::Create(tree, layer_impl_id_++, std::forward<Args>(args)...);
      T* ptr = layer.get();
      tree->root_layer_for_testing()->test_properties()->AddChild(
          std::move(layer));
      return ptr;
    }

    FakeLayerTreeHostClient client_;
    TestTaskGraphRunner task_graph_runner_;
    std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink_;
    std::unique_ptr<AnimationHost> animation_host_;
    std::unique_ptr<FakeLayerTreeHost> host_;
    std::unique_ptr<viz::RenderPass> render_pass_;
    scoped_refptr<AnimationTimeline> timeline_;
    scoped_refptr<AnimationTimeline> timeline_impl_;
    int layer_impl_id_;
    std::unique_ptr<RenderSurfaceList> render_surface_list_impl_;
    LayerImplList update_layer_impl_list_;
  };
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TEST_COMMON_H_
