// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MOCK_SHARED_IMAGE_VIDEO_PROVIDER_H_
#define MEDIA_GPU_ANDROID_MOCK_SHARED_IMAGE_VIDEO_PROVIDER_H_

#include "media/gpu/android/pooled_shared_image_video_provider.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MockSharedImageVideoProvider : public SharedImageVideoProvider {
 public:
  MockSharedImageVideoProvider();
  ~MockSharedImageVideoProvider();

  void Initialize(GpuInitCB gpu_init_cb) {
    Initialize_(gpu_init_cb);
    gpu_init_cb_ = std::move(gpu_init_cb);
  }

  MOCK_METHOD1(Initialize_, void(GpuInitCB& gpu_init_cb));

  void RequestImage(ImageReadyCB cb,
                    const ImageSpec& spec,
                    scoped_refptr<gpu::TextureOwner> texture_owner) override {
    requests_.emplace_back(std::move(cb), spec, std::move(texture_owner));

    MockRequestImage();
  }

  MOCK_METHOD0(MockRequestImage, void());

  // Provide an image for the least recent request.  If |record| is provided,
  // then we'll use it.  Otherwise, we'll make one up.
  void ProvideOneRequestedImage(ImageRecord* record = nullptr) {
    ImageRecord tmp_record;
    if (!record) {
      record = &tmp_record;
      record->release_cb = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              [](int* count, const gpu::SyncToken& sync_token) { (*count)++; },
              base::Unretained(&num_release_callbacks_)),
          gpu::SyncToken());
    }

    auto& req = requests_.front();
    std::move(req.cb_).Run(std::move(*record));
    requests_.pop_front();
  }

  // Most recent arguments to Initialize.
  GpuInitCB gpu_init_cb_;

  // Most recent arguments to RequestImage.
  struct RequestImageArgs {
    RequestImageArgs(ImageReadyCB cb,
                     ImageSpec spec,
                     scoped_refptr<gpu::TextureOwner> texture_owner);
    ~RequestImageArgs();
    ImageReadyCB cb_;
    ImageSpec spec_;
    scoped_refptr<gpu::TextureOwner> texture_owner_;
  };

  std::list<RequestImageArgs> requests_;

  // Number of times a release callback has been called.
  int num_release_callbacks_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MOCK_SHARED_IMAGE_VIDEO_PROVIDER_H_
