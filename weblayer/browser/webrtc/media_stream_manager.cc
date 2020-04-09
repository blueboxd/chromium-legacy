// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webrtc/media_stream_manager.h"

#include "base/supports_user_data.h"
#include "components/webrtc/media_stream_devices_controller.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/java/jni/MediaStreamManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

namespace {

constexpr int kWebContentsUserDataKey = 0;

struct UserData : public base::SupportsUserData::Data {
  MediaStreamManager* manager = nullptr;
};

}  // namespace

// A class that tracks the lifecycle of a single active media stream. Ownership
// is passed off to MediaResponseCallback.
class MediaStreamManager::StreamUi : public content::MediaStreamUI {
 public:
  StreamUi(MediaStreamManager* manager,
           const blink::MediaStreamDevices& devices)
      : manager_(manager) {
    DCHECK(manager_);
    for (const auto& device : devices) {
      if (device.type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE)
        streaming_audio_ = true;
      if (device.type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE)
        streaming_video_ = true;
    }
  }
  StreamUi(const StreamUi&) = delete;
  StreamUi& operator=(const StreamUi&) = delete;

  ~StreamUi() override {
    if (manager_)
      manager_->UnregisterStream(this);
  }

  // content::MediaStreamUi:
  gfx::NativeViewId OnStarted(base::OnceClosure stop,
                              SourceCallback source) override {
    if (manager_)
      manager_->RegisterStream(this);
    return 0;
  }

  void OnManagerGone() { manager_ = nullptr; }

  bool streaming_audio() const { return streaming_audio_; }

  bool streaming_video() const { return streaming_video_; }

 private:
  MediaStreamManager* manager_;
  bool streaming_audio_ = false;
  bool streaming_video_ = false;
};

MediaStreamManager::MediaStreamManager(
    const JavaParamRef<jobject>& j_object,
    const JavaParamRef<jobject>& j_web_contents)
    : j_object_(j_object) {
  auto user_data = std::make_unique<UserData>();
  user_data->manager = this;
  content::WebContents::FromJavaWebContents(j_web_contents)
      ->SetUserData(&kWebContentsUserDataKey, std::move(user_data));
}

MediaStreamManager::~MediaStreamManager() {
  for (auto* stream : active_streams_)
    stream->OnManagerGone();
}

// static
MediaStreamManager* MediaStreamManager::FromWebContents(
    content::WebContents* contents) {
  UserData* user_data = reinterpret_cast<UserData*>(
      contents->GetUserData(&kWebContentsUserDataKey));
  DCHECK(user_data);
  return user_data->manager;
}

void MediaStreamManager::RequestMediaAccessPermission(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  webrtc::MediaStreamDevicesController::RequestPermissions(
      request, nullptr,
      base::BindOnce(&MediaStreamManager::OnMediaAccessPermissionResult,
                     weak_factory_.GetWeakPtr(),
                     base::Passed(std::move(callback))));
}

void MediaStreamManager::OnMediaAccessPermissionResult(
    content::MediaResponseCallback callback,
    const blink::MediaStreamDevices& devices,
    blink::mojom::MediaStreamRequestResult result,
    bool blocked_by_feature_policy,
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  std::move(callback).Run(devices, result,
                          std::make_unique<StreamUi>(this, devices));
}

void MediaStreamManager::RegisterStream(StreamUi* stream) {
  active_streams_.insert(stream);
  Update();
}

void MediaStreamManager::UnregisterStream(StreamUi* stream) {
  active_streams_.erase(stream);
  Update();
}

void MediaStreamManager::Update() {
  bool audio = false;
  bool video = false;
  for (const auto* stream : active_streams_) {
    audio = audio || stream->streaming_audio();
    video = video || stream->streaming_video();
  }

  Java_MediaStreamManager_update(base::android::AttachCurrentThread(),
                                 j_object_, audio, video);
}

static jlong JNI_MediaStreamManager_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_object,
    const JavaParamRef<jobject>& j_web_contents) {
  return reinterpret_cast<intptr_t>(
      new MediaStreamManager(j_object, j_web_contents));
}

static void JNI_MediaStreamManager_Destroy(JNIEnv* env, jlong native_manager) {
  delete reinterpret_cast<MediaStreamManager*>(native_manager);
}

}  // namespace weblayer
