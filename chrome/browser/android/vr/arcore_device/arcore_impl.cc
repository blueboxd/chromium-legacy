// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_impl.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/numerics/math_constants.h"
#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/vr/arcore_device/arcore_java_utils.h"
#include "chrome/browser/android/vr/arcore_device/type_converters.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"

using base::android::JavaRef;

namespace {

device::mojom::VRPosePtr GetMojomPoseFromArPose(
    ArSession* session,
    device::internal::ScopedArCoreObject<ArPose*> pose) {
  float pose_raw[7];  // 7 = orientation(4) + position(3).
  ArPose_getPoseRaw(session, pose.get(), pose_raw);

  device::mojom::VRPosePtr result = device::mojom::VRPose::New();

  result->orientation =
      gfx::Quaternion(pose_raw[0], pose_raw[1], pose_raw[2], pose_raw[3]);
  result->position = gfx::Point3F(pose_raw[4], pose_raw[5], pose_raw[6]);

  return result;
}

}  // namespace

namespace device {

ArCoreImpl::ArCoreImpl()
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {}

ArCoreImpl::~ArCoreImpl() = default;

bool ArCoreImpl::Initialize(
    base::android::ScopedJavaLocalRef<jobject> context) {
  DCHECK(IsOnGlThread());
  DCHECK(!arcore_session_.is_valid());

  // TODO(https://crbug.com/837944): Notify error earlier if this will fail.

  JNIEnv* env = base::android::AttachCurrentThread();
  if (!env) {
    DLOG(ERROR) << "Unable to get JNIEnv for ArCore";
    return false;
  }

  // Use a local scoped ArSession for the next steps, we want the
  // arcore_session_ member to remain null until we complete successful
  // initialization.
  internal::ScopedArCoreObject<ArSession*> session;

  ArStatus status = ArSession_create(
      env, context.obj(),
      internal::ScopedArCoreObject<ArSession*>::Receiver(session).get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_create failed: " << status;
    return false;
  }

  // Set incognito mode for ARCore session - this is done unconditionally as we
  // always want to limit the amount of logging done by ARCore.
  ArSession_enableIncognitoMode_private(session.get());
  DVLOG(1) << __func__ << ": ARCore incognito mode enabled";

  internal::ScopedArCoreObject<ArConfig*> arcore_config;
  ArConfig_create(
      session.get(),
      internal::ScopedArCoreObject<ArConfig*>::Receiver(arcore_config).get());
  if (!arcore_config.is_valid()) {
    DLOG(ERROR) << "ArConfig_create failed";
    return false;
  }

  status = ArSession_configure(session.get(), arcore_config.get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_configure failed: " << status;
    return false;
  }

  internal::ScopedArCoreObject<ArFrame*> frame;
  ArFrame_create(session.get(),
                 internal::ScopedArCoreObject<ArFrame*>::Receiver(frame).get());
  if (!frame.is_valid()) {
    DLOG(ERROR) << "ArFrame_create failed";
    return false;
  }

  // Success, we now have a valid session and a valid frame.
  arcore_frame_ = std::move(frame);
  arcore_session_ = std::move(session);
  return true;
}

void ArCoreImpl::SetCameraTexture(GLuint camera_texture_id) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  ArSession_setCameraTextureName(arcore_session_.get(), camera_texture_id);
}

void ArCoreImpl::SetDisplayGeometry(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  // Display::Rotation is the same as Android's rotation and is compatible with
  // what ArCore is expecting.
  ArSession_setDisplayGeometry(arcore_session_.get(), display_rotation,
                               frame_size.width(), frame_size.height());
}

std::vector<float> ArCoreImpl::TransformDisplayUvCoords(
    const base::span<const float> uvs) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  size_t num_elements = uvs.size();
  DCHECK(num_elements % 2 == 0);
  std::vector<float> uvs_out(num_elements);

  ArFrame_transformCoordinates2d(
      arcore_session_.get(), arcore_frame_.get(),
      AR_COORDINATES_2D_VIEW_NORMALIZED, num_elements / 2, &uvs[0],
      AR_COORDINATES_2D_TEXTURE_NORMALIZED, &uvs_out[0]);
  return uvs_out;
}

mojom::VRPosePtr ArCoreImpl::Update(bool* camera_updated) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());
  DCHECK(camera_updated);

  ArStatus status;

  status = ArSession_update(arcore_session_.get(), arcore_frame_.get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_update failed: " << status;
    *camera_updated = false;
    return nullptr;
  }

  // If we get here, assume we have a valid camera image, but we don't know yet
  // if tracking is working.
  *camera_updated = true;
  internal::ScopedArCoreObject<ArCamera*> arcore_camera;
  ArFrame_acquireCamera(
      arcore_session_.get(), arcore_frame_.get(),
      internal::ScopedArCoreObject<ArCamera*>::Receiver(arcore_camera).get());
  if (!arcore_camera.is_valid()) {
    DLOG(ERROR) << "ArFrame_acquireCamera failed!";
    return nullptr;
  }

  ArTrackingState tracking_state;
  ArCamera_getTrackingState(arcore_session_.get(), arcore_camera.get(),
                            &tracking_state);
  if (tracking_state != AR_TRACKING_STATE_TRACKING) {
    DVLOG(1) << "Tracking state is not AR_TRACKING_STATE_TRACKING: "
             << tracking_state;
    return nullptr;
  }

  internal::ScopedArCoreObject<ArPose*> arcore_pose;
  ArPose_create(
      arcore_session_.get(), nullptr,
      internal::ScopedArCoreObject<ArPose*>::Receiver(arcore_pose).get());
  if (!arcore_pose.is_valid()) {
    DLOG(ERROR) << "ArPose_create failed!";
    return nullptr;
  }

  ArCamera_getDisplayOrientedPose(arcore_session_.get(), arcore_camera.get(),
                                  arcore_pose.get());

  return GetMojomPoseFromArPose(arcore_session_.get(), std::move(arcore_pose));
}

void ArCoreImpl::EnsureArCorePlanesList() {
  if (!arcore_planes_.is_valid()) {
    ArTrackableList_create(
        arcore_session_.get(),
        internal::ScopedArCoreObject<ArTrackableList*>::Receiver(arcore_planes_)
            .get());
    DCHECK(arcore_planes_.is_valid());
  }
}

template <typename FunctionType>
void ArCoreImpl::ForEachArCorePlane(FunctionType fn) {
  DCHECK(arcore_planes_.is_valid());

  int32_t trackable_list_size;
  ArTrackableList_getSize(arcore_session_.get(), arcore_planes_.get(),
                          &trackable_list_size);

  for (int i = 0; i < trackable_list_size; i++) {
    internal::ScopedArCoreObject<ArTrackable*> trackable;
    ArTrackableList_acquireItem(
        arcore_session_.get(), arcore_planes_.get(), i,
        internal::ScopedArCoreObject<ArTrackable*>::Receiver(trackable).get());

    ArTrackingState tracking_state;
    ArTrackable_getTrackingState(arcore_session_.get(), trackable.get(),
                                 &tracking_state);

    if (tracking_state != ArTrackingState::AR_TRACKING_STATE_TRACKING) {
      // Skip all planes that are not currently tracked.
      continue;
    }

#if DCHECK_IS_ON()
    {
      ArTrackableType type;
      ArTrackable_getType(arcore_session_.get(), trackable.get(), &type);
      DCHECK(type == ArTrackableType::AR_TRACKABLE_PLANE)
          << "arcore_planes_ contains a trackable that is not an ArPlane!";
    }
#endif

    ArPlane* ar_plane =
        ArAsPlane(trackable.get());  // Naked pointer is fine here, ArAsPlane
                                     // does not increase ref count.

    internal::ScopedArCoreObject<ArPlane*> subsuming_plane;
    ArPlane_acquireSubsumedBy(
        arcore_session_.get(), ar_plane,
        internal::ScopedArCoreObject<ArPlane*>::Receiver(subsuming_plane)
            .get());

    if (subsuming_plane.is_valid()) {
      // Current plane was subsumed by other plane, skip this loop iteration.
      // Subsuming plane will be handled when its turn comes.
      continue;
    }

    fn(ar_plane);
  }
}  // namespace device

std::vector<mojom::XRPlaneDataPtr> ArCoreImpl::GetUpdatedPlanesData() {
  EnsureArCorePlanesList();

  std::vector<mojom::XRPlaneDataPtr> result;

  ArTrackableType plane_tracked_type = AR_TRACKABLE_PLANE;
  ArFrame_getUpdatedTrackables(arcore_session_.get(), arcore_frame_.get(),
                               plane_tracked_type, arcore_planes_.get());

  ForEachArCorePlane([this, &result](ArPlane* ar_plane) {
    // orientation
    ArPlaneType plane_type;
    ArPlane_getType(arcore_session_.get(), ar_plane, &plane_type);

    // pose
    internal::ScopedArCoreObject<ArPose*> plane_pose;
    ArPose_create(
        arcore_session_.get(), nullptr,
        internal::ScopedArCoreObject<ArPose*>::Receiver(plane_pose).get());
    ArPlane_getCenterPose(arcore_session_.get(), ar_plane, plane_pose.get());
    mojom::VRPosePtr pose =
        GetMojomPoseFromArPose(arcore_session_.get(), std::move(plane_pose));

    // polygon
    int32_t polygon_size;
    ArPlane_getPolygonSize(arcore_session_.get(), ar_plane, &polygon_size);
    // We are supposed to get 2*N floats describing (x, z) cooridinates of N
    // points.
    DCHECK(polygon_size % 2 == 0);

    std::unique_ptr<float[]> vertices_raw =
        std::make_unique<float[]>(polygon_size);
    ArPlane_getPolygon(arcore_session_.get(), ar_plane, vertices_raw.get());

    std::vector<mojom::XRPlanePointDataPtr> vertices;
    for (int i = 0; i < polygon_size; i += 2) {
      vertices.push_back(
          mojom::XRPlanePointData::New(vertices_raw[i], vertices_raw[i + 1]));
    }

    // id
    int32_t plane_id;
    bool created;
    std::tie(plane_id, created) = CreateOrGetPlaneId(ar_plane);

    result.push_back(mojom::XRPlaneData::New(
        plane_id,
        mojo::ConvertTo<device::mojom::XRPlaneOrientation>(plane_type),
        std::move(pose), std::move(vertices)));
  });

  return result;
}

std::vector<int32_t> ArCoreImpl::GetAllPlaneIds() {
  EnsureArCorePlanesList();

  std::vector<int32_t> result;

  ArTrackableType plane_tracked_type = AR_TRACKABLE_PLANE;
  ArSession_getAllTrackables(arcore_session_.get(), plane_tracked_type,
                             arcore_planes_.get());

  ForEachArCorePlane([this, &result](ArPlane* ar_plane) {
    // id
    int32_t plane_id;
    bool created;
    std::tie(plane_id, created) = CreateOrGetPlaneId(ar_plane);

    // Newly detected planes should be handled by GetUpdatedPlanesData().
    DCHECK(!created);

    result.emplace_back(plane_id);
  });

  return result;
}

mojom::XRPlaneDetectionDataPtr ArCoreImpl::GetDetectedPlanesData() {
  TRACE_EVENT0("gpu", __FUNCTION__);

  std::vector<mojom::XRPlaneDataPtr> updated_planes = GetUpdatedPlanesData();
  std::vector<int32_t> all_plane_ids = GetAllPlaneIds();

  return mojom::XRPlaneDetectionData::New(all_plane_ids,
                                          std::move(updated_planes));
}

std::pair<int32_t, bool> ArCoreImpl::CreateOrGetPlaneId(void* plane_address) {
  auto it = ar_plane_address_to_id_.find(plane_address);
  if (it != ar_plane_address_to_id_.end()) {
    return std::make_pair(it->second, false);
  }

  // Make sure that incrementing next_id_ won't cause an overflow.
  CHECK(next_id_ != std::numeric_limits<int32_t>::max());

  int32_t current_id = next_id_++;
  ar_plane_address_to_id_.emplace(plane_address, current_id);

  return std::make_pair(current_id, true);
}

void ArCoreImpl::Pause() {
  DVLOG(2) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());

  ArStatus status = ArSession_pause(arcore_session_.get());
  DLOG_IF(ERROR, status != AR_SUCCESS)
      << "ArSession_pause failed: status = " << status;
}

void ArCoreImpl::Resume() {
  DVLOG(2) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());

  ArStatus status = ArSession_resume(arcore_session_.get());
  DLOG_IF(ERROR, status != AR_SUCCESS)
      << "ArSession_resume failed: status = " << status;
}

gfx::Transform ArCoreImpl::GetProjectionMatrix(float near, float far) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  internal::ScopedArCoreObject<ArCamera*> arcore_camera;
  ArFrame_acquireCamera(
      arcore_session_.get(), arcore_frame_.get(),
      internal::ScopedArCoreObject<ArCamera*>::Receiver(arcore_camera).get());
  DCHECK(arcore_camera.is_valid())
      << "ArFrame_acquireCamera failed despite documentation saying it cannot";

  // ArCore's projection matrix is 16 floats in column-major order.
  float matrix_4x4[16];
  ArCamera_getProjectionMatrix(arcore_session_.get(), arcore_camera.get(), near,
                               far, matrix_4x4);
  gfx::Transform result;
  result.matrix().setColMajorf(matrix_4x4);
  return result;
}

bool ArCoreImpl::RequestHitTest(
    const mojom::XRRayPtr& ray,
    std::vector<mojom::XRHitResultPtr>* hit_results) {
  DVLOG(2) << __func__ << ": ray origin=" << ray->origin.ToString()
           << ", direction=" << ray->direction.ToString();

  DCHECK(hit_results);
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  internal::ScopedArCoreObject<ArHitResultList*> arcore_hit_result_list;
  ArHitResultList_create(
      arcore_session_.get(),
      internal::ScopedArCoreObject<ArHitResultList*>::Receiver(
          arcore_hit_result_list)
          .get());
  if (!arcore_hit_result_list.is_valid()) {
    DLOG(ERROR) << "ArHitResultList_create failed!";
    return false;
  }

  // ArCore returns hit-results in sorted order, thus providing the guarantee
  // of sorted results promised by the WebXR spec for requestHitTest().
  float origin[3] = {ray->origin.x(), ray->origin.y(), ray->origin.z()};
  float direction[3] = {ray->direction.x(), ray->direction.y(),
                        ray->direction.z()};

  ArFrame_hitTestRay(arcore_session_.get(), arcore_frame_.get(), origin,
                     direction, arcore_hit_result_list.get());

  int arcore_hit_result_list_size = 0;
  ArHitResultList_getSize(arcore_session_.get(), arcore_hit_result_list.get(),
                          &arcore_hit_result_list_size);
  DVLOG(2) << __func__
           << ": arcore_hit_result_list_size=" << arcore_hit_result_list_size;

  // Go through the list in reverse order so the first hit we encounter is the
  // furthest.
  // We will accept the furthest hit and then for the rest require that the hit
  // be within the actual polygon detected by ArCore. This heuristic allows us
  // to get better results on floors w/o overestimating the size of tables etc.
  // See https://crbug.com/872855.

  for (int i = arcore_hit_result_list_size - 1; i >= 0; i--) {
    internal::ScopedArCoreObject<ArHitResult*> arcore_hit;

    ArHitResult_create(
        arcore_session_.get(),
        internal::ScopedArCoreObject<ArHitResult*>::Receiver(arcore_hit).get());

    if (!arcore_hit.is_valid()) {
      DLOG(ERROR) << "ArHitResult_create failed!";
      return false;
    }

    ArHitResultList_getItem(arcore_session_.get(), arcore_hit_result_list.get(),
                            i, arcore_hit.get());

    internal::ScopedArCoreObject<ArTrackable*> ar_trackable;

    ArHitResult_acquireTrackable(
        arcore_session_.get(), arcore_hit.get(),
        internal::ScopedArCoreObject<ArTrackable*>::Receiver(ar_trackable)
            .get());
    ArTrackableType ar_trackable_type = AR_TRACKABLE_NOT_VALID;
    ArTrackable_getType(arcore_session_.get(), ar_trackable.get(),
                        &ar_trackable_type);

    // Only consider hits with plane trackables
    // TODO(874985): make this configurable or re-evaluate this decision
    if (AR_TRACKABLE_PLANE != ar_trackable_type) {
      DVLOG(2) << __func__
               << ": hit a trackable that is not a plane, ignoring it";
      continue;
    }

    internal::ScopedArCoreObject<ArPose*> arcore_pose;
    ArPose_create(
        arcore_session_.get(), nullptr,
        internal::ScopedArCoreObject<ArPose*>::Receiver(arcore_pose).get());
    if (!arcore_pose.is_valid()) {
      DLOG(ERROR) << "ArPose_create failed!";
      return false;
    }

    ArHitResult_getHitPose(arcore_session_.get(), arcore_hit.get(),
                           arcore_pose.get());

    // After the first (furthest) hit, only return hits that are within the
    // actual detected polygon and not just within than the larger plane.
    if (!hit_results->empty()) {
      int32_t in_polygon = 0;
      ArPlane* ar_plane = ArAsPlane(ar_trackable.get());
      ArPlane_isPoseInPolygon(arcore_session_.get(), ar_plane,
                              arcore_pose.get(), &in_polygon);
      if (!in_polygon) {
        DVLOG(2) << __func__
                 << ": hit a trackable that is not within detected polygon, "
                    "ignoring it";
        continue;
      }
    }

    std::array<float, 16> matrix;
    ArPose_getMatrix(arcore_session_.get(), arcore_pose.get(), matrix.data());

    mojom::XRHitResultPtr mojo_hit = mojom::XRHitResult::New();

    // ArPose_getMatrix returns the matrix in WebGL style column-major order
    // and gfx::Transform expects row major order.
    // clang-format off
    mojo_hit->hit_matrix = gfx::Transform(
      matrix[0], matrix[4], matrix[8],  matrix[12],
      matrix[1], matrix[5], matrix[9],  matrix[13],
      matrix[2], matrix[6], matrix[10], matrix[14],
      matrix[3], matrix[7], matrix[11], matrix[15]
    );
    // clang-format on

    // Insert new results at head to preserver order from ArCore
    hit_results->insert(hit_results->begin(), std::move(mojo_hit));
  }

  DVLOG(2) << __func__ << ": hit_results->size()=" << hit_results->size();
  return true;
}

bool ArCoreImpl::IsOnGlThread() {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

std::unique_ptr<ArCore> ArCoreImplFactory::Create() {
  return std::make_unique<ArCoreImpl>();
}

}  // namespace device
