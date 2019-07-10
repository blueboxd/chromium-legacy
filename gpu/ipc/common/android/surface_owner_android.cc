// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/android/surface_owner_android.h"

#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/android/android_image_reader_utils.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace gpu {
namespace {

// Surface Texture based implementation.
class SurfaceTexture : public SurfaceOwner {
 public:
  explicit SurfaceTexture(uint32_t texture_id);
  ~SurfaceTexture() override;

  void SetFrameAvailableCallback(
      const base::RepeatingClosure& callback) override;
  gl::ScopedJavaSurface CreateJavaSurface() const override;
  void GetTransformMatrix(float mtx[16]) override;
  void UpdateTexImage() override;

 private:
  scoped_refptr<gl::SurfaceTexture> surface_texture_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTexture);
};

SurfaceTexture::SurfaceTexture(uint32_t texture_id)
    : surface_texture_(gl::SurfaceTexture::Create(texture_id)) {}

void SurfaceTexture::SetFrameAvailableCallback(
    const base::RepeatingClosure& callback) {
  surface_texture_->SetFrameAvailableCallback(callback);
}

gl::ScopedJavaSurface SurfaceTexture::CreateJavaSurface() const {
  return gl::ScopedJavaSurface(surface_texture_.get());
}

void SurfaceTexture::GetTransformMatrix(float mtx[]) {
  surface_texture_->GetTransformMatrix(mtx);
}

void SurfaceTexture::UpdateTexImage() {
  surface_texture_->UpdateTexImage();
}

SurfaceTexture::~SurfaceTexture() = default;

// ImageReader based implementation.
class ImageReader : public SurfaceOwner {
 public:
  explicit ImageReader(uint32_t texture_id);
  ~ImageReader() override;

  void SetFrameAvailableCallback(
      const base::RepeatingClosure& callback) override;
  gl::ScopedJavaSurface CreateJavaSurface() const override;
  void GetTransformMatrix(float mtx[16]) override;
  void UpdateTexImage() override;

 private:
  static void CallbackSignal(void* context, AImageReader* reader);

  // AImageReader instance
  AImageReader* image_reader_ = nullptr;

  // Image which was last acquired by the image reader. This needs to be saved
  // so that it can be deleted later.
  AImage* current_image_ = nullptr;

  GLuint texture_id_;

  // reference to the class instance which is used to dynamically
  // load the functions in android libraries at runtime.
  base::android::AndroidImageReader& loader_;

  // Frame available callback handling. ImageListner registered with
  // AImageReader is notified when there is a new frame available which
  // in turns runs the callback function on correct thread using task_runner.
  base::RepeatingClosure callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ImageReader);
};

ImageReader::ImageReader(GLuint texture_id)
    : texture_id_(texture_id),
      loader_(base::android::AndroidImageReader::GetInstance()) {
  // Set the width, height and format to some default value. This parameters
  // are/maybe overriden by the producer sending buffers to this imageReader's
  // Surface.
  // Note that max_images should be as small as possible to limit the memory
  // usage. ImageReader needs 2 images to mimic the behavior of SurfaceTexture.
  // Also note that we always acquire an image before deleting the
  // previous acquired image. This causes 2 acquired images to be in flight at
  // the image acquisition point until the previous image is deleted.
  int32_t width = 1, height = 1, max_images = 2;
  AIMAGE_FORMATS format = AIMAGE_FORMAT_YUV_420_888;
  AImageReader* reader = nullptr;
  // The usage flag below should be used when the buffer will be read from by
  // the GPU as a texture.
  const uint64_t usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

  // Create a new reader for images of the desired size and format.
  media_status_t return_code = loader_.AImageReader_newWithUsage(
      width, height, format, usage, max_images, &reader);
  if (return_code != AMEDIA_OK) {
    LOG(ERROR) << " Image reader creation failed.";
    if (return_code == AMEDIA_ERROR_INVALID_PARAMETER) {
      LOG(ERROR)
          << "Either reader is nullptr, or one or more of width, height, "
             "format, maxImages arguments is not supported";
    } else
      LOG(ERROR) << "unknown error";
    return;
  }
  DCHECK(reader);
  image_reader_ = reader;
}

// This callback function will be called when there is a new image available
// for in the image reader's queue.
void ImageReader::CallbackSignal(void* context, AImageReader* reader) {
  ImageReader* image_reader_ptr = reinterpret_cast<ImageReader*>(context);
  if (image_reader_ptr->task_runner_->BelongsToCurrentThread()) {
    image_reader_ptr->callback_.Run();
  } else {
    image_reader_ptr->task_runner_->PostTask(FROM_HERE,
                                             image_reader_ptr->callback_);
  }
}

void ImageReader::SetFrameAvailableCallback(
    const base::RepeatingClosure& callback) {
  callback_ = callback;
  task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Create a new Image Listner.
  AImageReader_ImageListener listener;
  listener.context = reinterpret_cast<void*>(this);
  listener.onImageAvailable = &ImageReader::CallbackSignal;

  // Set the onImageAvailable listener of this image reader.
  if (loader_.AImageReader_setImageListener(image_reader_, &listener) !=
      AMEDIA_OK) {
    LOG(ERROR) << " Failed to register AImageReader listener";
    return;
  }
}

ImageReader::~ImageReader() {
  loader_.AImageReader_setImageListener(image_reader_, nullptr);

  // Delete the image before closing the associated image reader.
  if (current_image_)
    loader_.AImage_delete(current_image_);

  // Delete the image reader.
  loader_.AImageReader_delete(image_reader_);
}

gl::ScopedJavaSurface ImageReader::CreateJavaSurface() const {
  // Get the android native window from the image reader.
  ANativeWindow* window = nullptr;
  if (loader_.AImageReader_getWindow(image_reader_, &window) != AMEDIA_OK) {
    LOG(ERROR) << "unable to get a window from image reader.";
    return gl::ScopedJavaSurface::AcquireExternalSurface(nullptr);
  }

  // Get the java surface object from the Android native window.
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject j_surface = loader_.ANativeWindow_toSurface(env, window);
  DCHECK(j_surface);

  // Get the scoped java surface that is owned externally.
  return gl::ScopedJavaSurface::AcquireExternalSurface(j_surface);
}

void ImageReader::UpdateTexImage() {
  DCHECK(image_reader_);

  // Acquire the latest image asynchronously
  AImage* image = nullptr;
  int acquire_fence_fd = -1;
  media_status_t return_code = AMEDIA_OK;

  // This method duplicates the fence file descriptor and the caller is
  // responsible for closing the returend file descriptor.
  // We now use AcquireNextImageAsync() instead of AcquireLatestImageAsync()
  // because AcquireLatestImageAsync() only works when
  // max_images-number_of_acquired_images >= 2. This is now not true for our
  // AImageReader use case with max_images=2 since we always have one previously
  // acquired image when we try to acquire a new image.
  return_code = loader_.AImageReader_acquireNextImageAsync(
      image_reader_, &image, &acquire_fence_fd);

  // Log the error return code.
  if (return_code != AMEDIA_OK) {
    base::UmaHistogramSparse("GPU.SurfaceOwner.AImageReader.AcquireImageResult",
                             return_code);
  }

  // TODO(http://crbug.com/846050).
  // Need to add some better error handling if below error occurs. Currently we
  // just return if error occurs.
  switch (return_code) {
    case AMEDIA_ERROR_INVALID_PARAMETER:
      LOG(ERROR) << " Image is nullptr";
      return;
    case AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED:
      LOG(ERROR)
          << "number of concurrently acquired images has reached the limit";
      return;
    case AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE:
      LOG(ERROR) << "no buffers currently available in the reader queue";
      return;
    case AMEDIA_ERROR_UNKNOWN:
      LOG(ERROR) << "method fails for some other reasons";
      return;
    case AMEDIA_OK:
      // Method call succeeded.
      break;
    default:
      // No other error code should be returned.
      NOTREACHED();
      return;
  }
  base::ScopedFD scoped_acquire_fence_fd(acquire_fence_fd);

  // If there is no new image simply return. At this point previous image will
  // still be bound to the texture.
  if (!image) {
    return;
  }

  // If we have a new Image, delete the previously acquired image.
  if (!gpu::DeleteAImageAsync(current_image_, &loader_))
    return;

  // Make the newly acuired image as current image.
  current_image_ = image;

  if (!gpu::InsertEglFenceAndWait(std::move(scoped_acquire_fence_fd)))
    return;

  // Create EGL image from the AImage and bind it to the texture.
  if (!gpu::CreateAndBindEglImage(current_image_, texture_id_, &loader_))
    return;
}

void ImageReader::GetTransformMatrix(float mtx[]) {
  // Assign a Y inverted Identity matrix. MediaPlayer path performs a Y
  // inversion of this matrix later. Hence if we assign a Y inverted matrix
  // here, it simply becomes an identity matrix later and will have no effect
  // on the image data.
  static constexpr float kYInvertedIdentity[16]{1, 0, 0, 0, 0, -1, 0, 0,
                                                0, 0, 1, 0, 0, 1,  0, 1};
  memcpy(mtx, kYInvertedIdentity, sizeof(kYInvertedIdentity));
}

}  // anonymous namespace

// Interface definitions.
SurfaceOwner::SurfaceOwner() = default;
SurfaceOwner::~SurfaceOwner() = default;

// Static.
std::unique_ptr<SurfaceOwner> SurfaceOwner::Create(uint32_t texture_id) {
  // Use AImageReader if its supported and is enabled by the feature flag.
  if (base::android::AndroidImageReader::GetInstance().IsSupported() &&
      base::FeatureList::IsEnabled(features::kAImageReaderMediaPlayer)) {
    return std::make_unique<ImageReader>(texture_id);
  }

  // If not, fall back to legacy path of using SurfaceTexture.
  return std::make_unique<SurfaceTexture>(texture_id);
}

}  // namespace gpu
