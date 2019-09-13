// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/status_indicator_scene_layer.h"

#include "cc/layers/solid_color_layer.h"
#include "chrome/android/chrome_jni_headers/StatusIndicatorSceneLayer_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

StatusIndicatorSceneLayer::StatusIndicatorSceneLayer(
    JNIEnv* env,
    const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      view_container_(cc::Layer::Create()),
      view_layer_(cc::SolidColorLayer::Create()) {
  layer()->SetIsDrawable(true);

  view_container_->SetIsDrawable(true);
  view_container_->SetMasksToBounds(true);

  view_layer_->SetIsDrawable(true);
  view_layer_->SetBackgroundColor(SK_ColorRED);
  view_container_->AddChild(view_layer_);
}

StatusIndicatorSceneLayer::~StatusIndicatorSceneLayer() {}

void StatusIndicatorSceneLayer::UpdateStatusIndicatorLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& object,
    const base::android::JavaParamRef<jobject>& jresource_manager,
    jint view_resource_id) {
  // This is temporary as the size should come from the resource.
  view_container_->SetBounds(gfx::Size(1440, 70));
  view_container_->SetPosition(gfx::PointF(0, 0));

  // The view's layer should be the same size as the texture.
  view_layer_->SetBounds(gfx::Size(gfx::Size(1440, 70)));
  view_layer_->SetPosition(gfx::PointF(0, 0));
}

void StatusIndicatorSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer())
    return;

  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != layer_->id())) {
    layer_->AddChild(content_tree->layer());
    layer_->AddChild(view_container_);
  }
}

static jlong JNI_StatusIndicatorSceneLayer_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  StatusIndicatorSceneLayer* scene_layer =
      new StatusIndicatorSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
