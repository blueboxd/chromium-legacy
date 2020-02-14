// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_plane_manager.h"

#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/vr/arcore_device/type_converters.h"

namespace device {

std::pair<gfx::Quaternion, gfx::Point3F> GetPositionAndOrientationFromArPose(
    const ArSession* session,
    const ArPose* pose) {
  std::array<float, 7> pose_raw;  // 7 = orientation(4) + position(3).
  ArPose_getPoseRaw(session, pose, pose_raw.data());

  return {gfx::Quaternion(pose_raw[0], pose_raw[1], pose_raw[2], pose_raw[3]),
          gfx::Point3F(pose_raw[4], pose_raw[5], pose_raw[6])};
}

device::mojom::Pose GetMojomPoseFromArPose(const ArSession* session,
                                           const ArPose* pose) {
  device::mojom::Pose result;
  std::tie(result.orientation, result.position) =
      GetPositionAndOrientationFromArPose(session, pose);

  return result;
}

device::internal::ScopedArCoreObject<ArPose*> GetArPoseFromMojomPose(
    const ArSession* session,
    const device::mojom::Pose& pose) {
  float pose_raw[7] = {};  // 7 = orientation(4) + position(3).

  pose_raw[0] = pose.orientation.x();
  pose_raw[1] = pose.orientation.y();
  pose_raw[2] = pose.orientation.z();
  pose_raw[3] = pose.orientation.w();

  pose_raw[4] = pose.position.x();
  pose_raw[5] = pose.position.y();
  pose_raw[6] = pose.position.z();

  device::internal::ScopedArCoreObject<ArPose*> result;

  ArPose_create(
      session, pose_raw,
      device::internal::ScopedArCoreObject<ArPose*>::Receiver(result).get());

  return result;
}

ArCorePlaneManager::ArCorePlaneManager(util::PassKey<ArCoreImpl> pass_key,
                                       ArSession* arcore_session)
    : arcore_session_(arcore_session) {
  DCHECK(arcore_session_);

  ArTrackableList_create(
      arcore_session_,
      internal::ScopedArCoreObject<ArTrackableList*>::Receiver(arcore_planes_)
          .get());
  DCHECK(arcore_planes_.is_valid());

  ArPose_create(
      arcore_session_, nullptr,
      internal::ScopedArCoreObject<ArPose*>::Receiver(ar_pose_).get());
  DCHECK(ar_pose_.is_valid());
}

ArCorePlaneManager::~ArCorePlaneManager() = default;

template <typename FunctionType>
void ArCorePlaneManager::ForEachArCorePlane(FunctionType fn) {
  DCHECK(arcore_planes_.is_valid());

  int32_t trackable_list_size;
  ArTrackableList_getSize(arcore_session_, arcore_planes_.get(),
                          &trackable_list_size);

  for (int i = 0; i < trackable_list_size; i++) {
    internal::ScopedArCoreObject<ArTrackable*> trackable;
    ArTrackableList_acquireItem(
        arcore_session_, arcore_planes_.get(), i,
        internal::ScopedArCoreObject<ArTrackable*>::Receiver(trackable).get());

    ArTrackingState tracking_state;
    ArTrackable_getTrackingState(arcore_session_, trackable.get(),
                                 &tracking_state);

    if (tracking_state != ArTrackingState::AR_TRACKING_STATE_TRACKING) {
      // Skip all planes that are not currently tracked.
      continue;
    }

#if DCHECK_IS_ON()
    {
      ArTrackableType type;
      ArTrackable_getType(arcore_session_, trackable.get(), &type);
      DCHECK(type == ArTrackableType::AR_TRACKABLE_PLANE)
          << "arcore_planes_ contains a trackable that is not an ArPlane!";
    }
#endif

    ArPlane* ar_plane =
        ArAsPlane(trackable.get());  // Naked pointer is fine here, ArAsPlane
                                     // does not increase ref count.

    internal::ScopedArCoreObject<ArPlane*> subsuming_plane;
    ArPlane_acquireSubsumedBy(
        arcore_session_, ar_plane,
        internal::ScopedArCoreObject<ArPlane*>::Receiver(subsuming_plane)
            .get());

    if (subsuming_plane.is_valid()) {
      // Current plane was subsumed by other plane, skip this loop iteration.
      // Subsuming plane will be handled when its turn comes.
      continue;
    }

    fn(std::move(trackable), ar_plane);
  }
}

std::vector<mojom::XRPlaneDataPtr> ArCorePlaneManager::GetUpdatedPlanesData(
    ArFrame* arcore_frame) {
  std::vector<mojom::XRPlaneDataPtr> result;

  ArTrackableType plane_tracked_type = AR_TRACKABLE_PLANE;
  ArFrame_getUpdatedTrackables(arcore_session_, arcore_frame,
                               plane_tracked_type, arcore_planes_.get());

  ForEachArCorePlane(
      [this, &result](internal::ScopedArCoreObject<ArTrackable*> trackable,
                      ArPlane* ar_plane) {
        // orientation
        ArPlaneType plane_type;
        ArPlane_getType(arcore_session_, ar_plane, &plane_type);

        // pose
        internal::ScopedArCoreObject<ArPose*> plane_pose;
        ArPose_create(
            arcore_session_, nullptr,
            internal::ScopedArCoreObject<ArPose*>::Receiver(plane_pose).get());
        ArPlane_getCenterPose(arcore_session_, ar_plane, plane_pose.get());
        mojom::Pose pose =
            GetMojomPoseFromArPose(arcore_session_, plane_pose.get());

        // polygon
        int32_t polygon_size;
        ArPlane_getPolygonSize(arcore_session_, ar_plane, &polygon_size);
        // We are supposed to get 2*N floats describing (x, z) cooridinates of N
        // points.
        DCHECK(polygon_size % 2 == 0);

        std::unique_ptr<float[]> vertices_raw =
            std::make_unique<float[]>(polygon_size);
        ArPlane_getPolygon(arcore_session_, ar_plane, vertices_raw.get());

        std::vector<mojom::XRPlanePointDataPtr> vertices;
        for (int i = 0; i < polygon_size; i += 2) {
          vertices.push_back(mojom::XRPlanePointData::New(vertices_raw[i],
                                                          vertices_raw[i + 1]));
        }

        // ID
        PlaneId plane_id;
        bool created;
        std::tie(plane_id, created) = CreateOrGetPlaneId(ar_plane);

        result.push_back(mojom::XRPlaneData::New(
            plane_id.GetUnsafeValue(),
            mojo::ConvertTo<device::mojom::XRPlaneOrientation>(plane_type),
            device::mojom::Pose::New(pose), std::move(vertices)));
      });

  return result;
}

std::vector<uint64_t> ArCorePlaneManager::GetAllPlaneIds() {
  std::vector<uint64_t> result;

  ArTrackableType plane_tracked_type = AR_TRACKABLE_PLANE;
  ArSession_getAllTrackables(arcore_session_, plane_tracked_type,
                             arcore_planes_.get());

  std::map<PlaneId, device::internal::ScopedArCoreObject<ArTrackable*>>
      plane_id_to_plane_object;

  ForEachArCorePlane([this, &plane_id_to_plane_object, &result](
                         internal::ScopedArCoreObject<ArTrackable*> trackable,
                         ArPlane* ar_plane) {
    // ID
    PlaneId plane_id;
    bool created;
    std::tie(plane_id, created) = CreateOrGetPlaneId(ar_plane);

    DCHECK(!created)
        << "Newly detected planes should be handled by GetUpdatedPlanesData().";

    result.emplace_back(plane_id);
    plane_id_to_plane_object[plane_id] = std::move(trackable);
  });

  plane_id_to_plane_object_.swap(plane_id_to_plane_object);

  return result;
}

mojom::XRPlaneDetectionDataPtr ArCorePlaneManager::GetDetectedPlanesData(
    ArFrame* ar_frame) {
  TRACE_EVENT0("gpu", __FUNCTION__);

  DCHECK(ar_frame);

  auto updated_planes = GetUpdatedPlanesData(ar_frame);
  auto all_plane_ids = GetAllPlaneIds();

  return mojom::XRPlaneDetectionData::New(all_plane_ids,
                                          std::move(updated_planes));
}

std::pair<PlaneId, bool> ArCorePlaneManager::CreateOrGetPlaneId(
    void* plane_address) {
  auto it = ar_plane_address_to_id_.find(plane_address);
  if (it != ar_plane_address_to_id_.end()) {
    return std::make_pair(it->second, false);
  }

  CHECK(next_id_ != std::numeric_limits<uint64_t>::max())
      << "preventing ID overflow";

  uint64_t current_id = next_id_++;
  ar_plane_address_to_id_.emplace(plane_address, current_id);

  return std::make_pair(PlaneId(current_id), true);
}

bool ArCorePlaneManager::PlaneExists(PlaneId id) {
  return base::Contains(plane_id_to_plane_object_, id);
}

base::Optional<gfx::Transform> ArCorePlaneManager::GetMojoFromPlane(
    PlaneId id) {
  auto it = plane_id_to_plane_object_.find(id);
  if (it == plane_id_to_plane_object_.end()) {
    return base::nullopt;
  }

  ArPlane* plane =
      ArAsPlane(it->second.get());  // Naked pointer is fine here, ArAsPlane
                                    // does not increase the internal refcount.

  ArPlane_getCenterPose(arcore_session_, plane, ar_pose_.get());
  mojom::Pose mojo_pose =
      GetMojomPoseFromArPose(arcore_session_, ar_pose_.get());

  return mojo::ConvertTo<gfx::Transform>(mojo_pose);
}

device::internal::ScopedArCoreObject<ArAnchor*>
ArCorePlaneManager::CreateAnchor(PlaneId id, const device::mojom::Pose& pose) {
  auto it = plane_id_to_plane_object_.find(id);
  if (it == plane_id_to_plane_object_.end()) {
    return {};
  }

  auto ar_pose = GetArPoseFromMojomPose(arcore_session_, pose);

  device::internal::ScopedArCoreObject<ArAnchor*> ar_anchor;
  ArStatus status = ArTrackable_acquireNewAnchor(
      arcore_session_, it->second.get(), ar_pose.get(),
      device::internal::ScopedArCoreObject<ArAnchor*>::Receiver(ar_anchor)
          .get());

  if (status != AR_SUCCESS) {
    return {};
  }

  return ar_anchor;
}

}  // namespace device
