// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/transform_node.h"

namespace cc {
namespace {

class FakeTextHolder : public TextHolder {
 public:
  FakeTextHolder(const std::string& text, const gfx::Rect& rect)
      : text_(text), rect_(rect) {}
  std::string text() const { return text_; }
  gfx::Rect rect() const { return rect_; }

 protected:
  ~FakeTextHolder() override = default;

 private:
  std::string text_;
  gfx::Rect rect_;
};

class FakeCaptureContentLayerClient : public FakeContentLayerClient {
 public:
  void addTextHolder(scoped_refptr<FakeTextHolder> holder) {
    holders_.push_back(holder);
  }

  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_control) override {
    auto display_list = base::MakeRefCounted<DisplayItemList>();
    for (auto holder : holders_) {
      display_list->StartPaint();
      display_list->push<DrawTextBlobOp>(
          SkTextBlob::MakeFromString(holder->text().data(), SkFont()),
          holder->rect().x(), holder->rect().y(), PaintFlags(),
          NodeHolder(holder));
      display_list->EndPaintOfUnpaired(holder->rect());
    }
    display_list->Finalize();
    return display_list;
  }

 private:
  std::vector<scoped_refptr<FakeTextHolder>> holders_;
};

// These tests are for LayerTreeHost::CaptureContent().
class LayerTreeHostCaptureContentTest : public LayerTreeTest {
 public:
  ~LayerTreeHostCaptureContentTest() override = default;

 protected:
  LayerTreeHostCaptureContentTest() : device_bounds_(10, 10) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override { PostCaptureContentToMainThread(); }

  void SetupRootPictureLayer(const gfx::Size& size) {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(size);

    client_.set_bounds(size);
    root_picture_layer_ = FakePictureLayer::Create(&client_);
    root_picture_layer_->SetBounds(size);
    root->AddChild(root_picture_layer_);

    layer_tree_host()->SetRootLayer(root);
    layer_tree_host()->SetViewportVisibleRect(gfx::Rect(device_bounds_));
  }

  void VerifyCapturedContent(
      std::vector<scoped_refptr<FakeTextHolder>>* expected_result) {
    EXPECT_EQ(expected_result->size(), captured_content_.size());
    size_t expected_left_result = expected_result->size();
    for (auto c : captured_content_) {
      EXPECT_EQ(c.type, NodeHolder::Type::kTextHolder);
      for (auto it = expected_result->begin(); it != expected_result->end();
           ++it) {
        if (it->get() == c.text_holder.get()) {
          EXPECT_EQ(static_cast<FakeTextHolder*>(c.text_holder.get())->text(),
                    it->get()->text());
          expected_result->erase(it);
          break;
        }
      }
      EXPECT_EQ(--expected_left_result, expected_result->size());
    }
  }

  FakeCaptureContentLayerClient client_;

 private:
  void PostCaptureContentToMainThread() {
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&LayerTreeHostCaptureContentTest::CaptureContent,
                       weak_factory_.GetWeakPtr()));
  }

  void CaptureContent() {
    captured_content_.clear();
    layer_tree_host()->CaptureContent(&captured_content_);
    EndTest();
  }

  scoped_refptr<FakePictureLayer> root_picture_layer_;
  std::vector<NodeHolder> captured_content_;
  const gfx::Size device_bounds_;
  base::WeakPtrFactory<LayerTreeHostCaptureContentTest> weak_factory_{this};
};

class LayerTreeHostCaptureContentTestBasic
    : public LayerTreeHostCaptureContentTest {
 protected:
  void SetupTextHolders(const gfx::Rect& rect1, const gfx::Rect& rect2) {
    text_holder_1_ = base::MakeRefCounted<FakeTextHolder>("Text1", rect1);
    client_.addTextHolder(text_holder_1_);
    text_holder_2_ = base::MakeRefCounted<FakeTextHolder>("Text2", rect2);
    client_.addTextHolder(text_holder_2_);
  }

  scoped_refptr<FakeTextHolder> text_holder_1_;
  scoped_refptr<FakeTextHolder> text_holder_2_;
};

// Test that one DrawTextBlobOp is on-screen, another isn't.
class LayerTreeHostCaptureContentTestOneVisible
    : public LayerTreeHostCaptureContentTestBasic {
 protected:
  void SetupTree() override {
    // One is visible.
    SetupTextHolders(gfx::Rect(0, 0, 5, 5), gfx::Rect(11, 0, 5, 5));
    SetupRootPictureLayer(gfx::Size(10, 10));
  }

  void AfterTest() override {
    std::vector<scoped_refptr<FakeTextHolder>> expected_result;
    expected_result.push_back(text_holder_1_);
    VerifyCapturedContent(&expected_result);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostCaptureContentTestOneVisible);

// Test that both DrawTextBlobOps are on-screen.
class LayerTreeHostCaptureContentTestTwoVisible
    : public LayerTreeHostCaptureContentTestBasic {
 protected:
  void SetupTree() override {
    // One is visible, another is partial visible.
    SetupTextHolders(gfx::Rect(0, 0, 5, 5), gfx::Rect(9, 0, 5, 5));
    SetupRootPictureLayer(gfx::Size(10, 10));
  }

  void AfterTest() override {
    std::vector<scoped_refptr<FakeTextHolder>> expected_result;
    expected_result.push_back(text_holder_1_);
    expected_result.push_back(text_holder_2_);
    VerifyCapturedContent(&expected_result);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostCaptureContentTestTwoVisible);

// Base class for two layers tests.
class LayerTreeHostCaptureContentTestTwoLayers
    : public LayerTreeHostCaptureContentTestBasic {
 protected:
  void SetupTree() override {
    SetupTextHolders(gfx::Rect(0, 0, 5, 5), gfx::Rect(5, 5, 5, 5));
    SetupRootPictureLayer(gfx::Size(10, 10));
    SetupSecondaryPictureLayer(gfx::Size(10, 10));
  }

  void DidCommit() override {
    LayerTreeHostCaptureContentTestBasic::DidCommit();
    OnSetupSecondaryLayTransform();
  }

  virtual void OnSetupSecondaryLayTransform() {}

  virtual bool IsSecondaryPictureLayerContentsOpaque() { return true; }

  // Setup transform node for secondary picture layer, must be called in
  // OnSetupSecondaryLayTransform().
  void SetupTransform(const gfx::Vector2dF& translate) {
    TransformNode transform_node;
    transform_node.source_node_id = 0;
    transform_node.local.Translate(translate);
    transform_node.id =
        layer_tree_host()->property_trees()->transform_tree.Insert(
            transform_node, 0);
    picture_layer->SetTransformTreeIndex(transform_node.id);
    layer_tree_host()->property_trees()->transform_tree.UpdateTransforms(
        transform_node.id);
  }

  void SetupSecondaryPictureLayer(const gfx::Size& size) {
    // Add text to layer.
    text_holder_21_ =
        base::MakeRefCounted<FakeTextHolder>("Text21", gfx::Rect(0, 0, 10, 5));
    client2_.addTextHolder(text_holder_21_);
    text_holder_22_ =
        base::MakeRefCounted<FakeTextHolder>("Text22", gfx::Rect(0, 5, 10, 5));
    client2_.addTextHolder(text_holder_22_);
    client2_.set_bounds(size);

    // Create layer.
    picture_layer = FakePictureLayer::Create(&client2_);
    picture_layer->SetBounds(size);
    picture_layer->SetContentsOpaque(IsSecondaryPictureLayerContentsOpaque());
    layer_tree_host()->root_layer()->AddChild(picture_layer);
  }

  scoped_refptr<FakePictureLayer> picture_layer;
  scoped_refptr<FakeTextHolder> text_holder_21_;
  scoped_refptr<FakeTextHolder> text_holder_22_;
  FakeCaptureContentLayerClient client2_;
};

// Test that one layer is within screen, another isn't.
class LayerTreeHostCaptureContentTestOneLayerVisible
    : public LayerTreeHostCaptureContentTestTwoLayers {
  void OnSetupSecondaryLayTransform() override {
    // Moves the layer out of visual port.
    SetupTransform(gfx::Vector2dF(0, 10));
  }

  void AfterTest() override {
    std::vector<scoped_refptr<FakeTextHolder>> expected_result;
    expected_result.push_back(text_holder_1_);
    expected_result.push_back(text_holder_2_);
    VerifyCapturedContent(&expected_result);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostCaptureContentTestOneLayerVisible);

// Test that the upper layer is partially on-screen, the under layer fully
// occupies the screen, all layer's on-screen content are returned even the
// upper layer is opaque.
class LayerTreeHostCaptureContentTestTwoLayersVisible
    : public LayerTreeHostCaptureContentTestTwoLayers {
  void OnSetupSecondaryLayTransform() override {
    SetupTransform(gfx::Vector2dF(0, 8));
  }

  void AfterTest() override {
    std::vector<scoped_refptr<FakeTextHolder>> expected_result;
    expected_result.push_back(text_holder_1_);
    expected_result.push_back(text_holder_2_);
    expected_result.push_back(text_holder_21_);
    VerifyCapturedContent(&expected_result);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostCaptureContentTestTwoLayersVisible);

// Test that the upper layer is transparent, the under layer fully occupies the
// screen, all layer's on-screen content are returned.
class LayerTreeHostCaptureContentTestTwoLayersVisibleAndTransparent
    : public LayerTreeHostCaptureContentTestTwoLayersVisible {
  bool IsSecondaryPictureLayerContentsOpaque() override { return false; }

  void AfterTest() override {
    // All 3  TextHolders are returned.
    std::vector<scoped_refptr<FakeTextHolder>> expected_result;
    expected_result.push_back(text_holder_1_);
    expected_result.push_back(text_holder_2_);
    expected_result.push_back(text_holder_21_);
    VerifyCapturedContent(&expected_result);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostCaptureContentTestTwoLayersVisibleAndTransparent);

// Test that the upper layer is partially visible, but doesn't overlay screen in
// either direction, the under layer's content will be captured even it is fully
// overlaid by the upper layer.
class LayerTreeHostCaptureContentTestUpperLayerPartialOverlay
    : public LayerTreeHostCaptureContentTestTwoLayers {
  void OnSetupSecondaryLayTransform() override {
    SetupTransform(gfx::Vector2dF(2, 2));
  }

  void AfterTest() override {
    std::vector<scoped_refptr<FakeTextHolder>> expected_result;
    expected_result.push_back(text_holder_1_);
    expected_result.push_back(text_holder_2_);
    expected_result.push_back(text_holder_21_);
    expected_result.push_back(text_holder_22_);
    VerifyCapturedContent(&expected_result);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostCaptureContentTestUpperLayerPartialOverlay);

}  // namespace
}  // namespace cc
