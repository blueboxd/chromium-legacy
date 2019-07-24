// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_generic.h"
#include "chrome/browser/android/vr/arcore_device/arcore.h"
#include "chrome/browser/android/vr/arcore_device/arcore_sdk.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {

namespace internal {

template <class T>
struct ScopedGenericArObject {
  static T InvalidValue() { return nullptr; }
  static void Free(T object) {}
};

template <>
void inline ScopedGenericArObject<ArSession*>::Free(ArSession* ar_session) {
  ArSession_destroy(ar_session);
}

template <>
void inline ScopedGenericArObject<ArFrame*>::Free(ArFrame* ar_frame) {
  ArFrame_destroy(ar_frame);
}

template <>
void inline ScopedGenericArObject<ArConfig*>::Free(ArConfig* ar_config) {
  ArConfig_destroy(ar_config);
}

template <>
void inline ScopedGenericArObject<ArPose*>::Free(ArPose* ar_pose) {
  ArPose_destroy(ar_pose);
}

template <>
void inline ScopedGenericArObject<ArTrackable*>::Free(
    ArTrackable* ar_trackable) {
  ArTrackable_release(ar_trackable);
}

template <>
void inline ScopedGenericArObject<ArPlane*>::Free(ArPlane* ar_plane) {
  // ArPlane itself doesn't have a method to decrease refcount, but it is an
  // instance of ArTrackable & we have to use ArTrackable_release.
  ArTrackable_release(ArAsTrackable(ar_plane));
}

template <>
void inline ScopedGenericArObject<ArTrackableList*>::Free(
    ArTrackableList* ar_trackable_list) {
  ArTrackableList_destroy(ar_trackable_list);
}

template <>
void inline ScopedGenericArObject<ArCamera*>::Free(ArCamera* ar_camera) {
  // Do nothing - ArCamera has no destroy method and is managed by ArCore.
}

template <>
void inline ScopedGenericArObject<ArHitResultList*>::Free(
    ArHitResultList* ar_hit_result_list) {
  ArHitResultList_destroy(ar_hit_result_list);
}

template <>
void inline ScopedGenericArObject<ArHitResult*>::Free(
    ArHitResult* ar_hit_result) {
  ArHitResult_destroy(ar_hit_result);
}

template <class T>
using ScopedArCoreObject = base::ScopedGeneric<T, ScopedGenericArObject<T>>;

}  // namespace internal

// This class should be created and accessed entirely on a Gl thread.
class ArCoreImpl : public ArCore {
 public:
  ArCoreImpl();
  ~ArCoreImpl() override;

  bool Initialize(
      base::android::ScopedJavaLocalRef<jobject> application_context) override;
  void SetDisplayGeometry(const gfx::Size& frame_size,
                          display::Display::Rotation display_rotation) override;
  void SetCameraTexture(GLuint camera_texture_id) override;
  std::vector<float> TransformDisplayUvCoords(
      const base::span<const float> uvs) override;
  gfx::Transform GetProjectionMatrix(float near, float far) override;
  mojom::VRPosePtr Update(bool* camera_updated) override;
  mojom::XRPlaneDetectionDataPtr GetDetectedPlanesData() override;
  void Pause() override;
  void Resume() override;
  bool RequestHitTest(const mojom::XRRayPtr& ray,
                      std::vector<mojom::XRHitResultPtr>* hit_results) override;

 private:
  bool IsOnGlThread();
  base::WeakPtr<ArCoreImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  // An ArCore session, which is distinct and independent of XRSessions.
  // There will only ever be one in Chrome even when supporting
  // multiple XRSessions.
  internal::ScopedArCoreObject<ArSession*> arcore_session_;
  internal::ScopedArCoreObject<ArFrame*> arcore_frame_;

  // List of trackables - used for retrieving planes detected by ARCore. The
  // list will initially be null - call EnsureArCorePlanesList() before using
  // it. Instead of creating it in every call to GetUpdatedPlanesData(), the
  // list can be reused as it will be cleaned by calls to ArCore SDK.
  internal::ScopedArCoreObject<ArTrackableList*> arcore_planes_;

  // Initializes |arcore_planes_| list.
  void EnsureArCorePlanesList();

  // Returns vector containing information about all planes updated in current
  // frame, assigning IDs for newly detected planes as needed.
  std::vector<mojom::XRPlaneDataPtr> GetUpdatedPlanesData();

  // This must be called after GetUpdatedPlanesData as it assumes that all
  // planes already have an ID assigned. The result includes freshly assigned
  // IDs for newly detected planes along with previously known IDs for updated
  // and unchanged planes. It excludes planes that are no longer being tracked.
  std::vector<int32_t> GetAllPlaneIds();

  int32_t next_id_ = 1;
  std::unordered_map<void*, int32_t> ar_plane_address_to_id_;

  // Returns tuple containing plane id and a boolean signifying that the plane
  // was created.
  std::pair<int32_t, bool> CreateOrGetPlaneId(void* plane_address);

  // Executes |fn| for each still tracked, non-subsumed plane present in
  // |arcore_planes_|.
  template <typename FunctionType>
  void ForEachArCorePlane(FunctionType fn);

  // Must be last.
  base::WeakPtrFactory<ArCoreImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ArCoreImpl);
};

class ArCoreImplFactory : public ArCoreFactory {
 public:
  std::unique_ptr<ArCore> Create() override;
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_
