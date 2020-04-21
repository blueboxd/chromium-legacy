// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/util/type_safety/id_type.h"
#include "chrome/browser/android/vr/arcore_device/arcore.h"
#include "chrome/browser/android/vr/arcore_device/arcore_anchor_manager.h"
#include "chrome/browser/android/vr/arcore_device/arcore_plane_manager.h"
#include "chrome/browser/android/vr/arcore_device/arcore_sdk.h"
#include "chrome/browser/android/vr/arcore_device/scoped_arcore_objects.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {

class ArCorePlaneManager;

using AnchorId = util::IdTypeU64<class AnchorTag>;
using HitTestSubscriptionId = util::IdTypeU64<class HitTestSubscriptionTag>;

struct HitTestSubscriptionData {
  mojom::XRNativeOriginInformationPtr native_origin_information;
  const std::vector<mojom::EntityTypeForHitTest> entity_types;
  mojom::XRRayPtr ray;

  HitTestSubscriptionData(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray);
  HitTestSubscriptionData(HitTestSubscriptionData&& other);
  ~HitTestSubscriptionData();
};

struct TransientInputHitTestSubscriptionData {
  const std::string profile_name;
  const std::vector<mojom::EntityTypeForHitTest> entity_types;
  mojom::XRRayPtr ray;

  TransientInputHitTestSubscriptionData(
      const std::string& profile_name,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray);
  TransientInputHitTestSubscriptionData(
      TransientInputHitTestSubscriptionData&& other);
  ~TransientInputHitTestSubscriptionData();
};

class CreateAnchorRequest {
 public:
  mojom::XRNativeOriginInformation GetNativeOriginInformation() const;
  gfx::Transform GetNativeOriginFromAnchor() const;

  ArCore::CreateAnchorCallback TakeCallback();

  CreateAnchorRequest(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const gfx::Transform& native_origin_from_anchor,
      ArCore::CreateAnchorCallback callback);
  CreateAnchorRequest(CreateAnchorRequest&& other);
  ~CreateAnchorRequest();

 private:
  const mojom::XRNativeOriginInformation native_origin_information_;
  const gfx::Transform native_origin_from_anchor_;

  ArCore::CreateAnchorCallback callback_;
};

class CreatePlaneAttachedAnchorRequest {
 public:
  uint64_t GetPlaneId() const;
  mojom::XRNativeOriginInformation GetNativeOriginInformation() const;
  gfx::Transform GetNativeOriginFromAnchor() const;

  ArCore::CreateAnchorCallback TakeCallback();

  CreatePlaneAttachedAnchorRequest(const gfx::Transform& plane_from_anchor,
                                   uint64_t plane_id,
                                   ArCore::CreateAnchorCallback callback);
  CreatePlaneAttachedAnchorRequest(CreatePlaneAttachedAnchorRequest&& other);
  ~CreatePlaneAttachedAnchorRequest();

 private:
  const gfx::Transform plane_from_anchor_;
  const uint64_t plane_id_;

  ArCore::CreateAnchorCallback callback_;
};

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
  base::TimeDelta GetFrameTimestamp() override;

  mojom::XRPlaneDetectionDataPtr GetDetectedPlanesData() override;

  mojom::XRAnchorsDataPtr GetAnchorsData() override;

  mojom::XRLightEstimationDataPtr GetLightEstimationData() override;

  void Pause() override;
  void Resume() override;

  float GetEstimatedFloorHeight() override;

  bool RequestHitTest(const mojom::XRRayPtr& ray,
                      std::vector<mojom::XRHitResultPtr>* hit_results) override;

  // Helper.
  bool RequestHitTest(
      const gfx::Point3F& origin,
      const gfx::Vector3dF& direction,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      std::vector<mojom::XRHitResultPtr>* hit_results);

  base::Optional<uint64_t> SubscribeToHitTest(
      mojom::XRNativeOriginInformationPtr nativeOriginInformation,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray) override;
  base::Optional<uint64_t> SubscribeToHitTestForTransientInput(
      const std::string& profile_name,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray) override;

  mojom::XRHitTestSubscriptionResultsDataPtr GetHitTestSubscriptionResults(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state) override;

  void UnsubscribeFromHitTest(uint64_t subscription_id) override;

  void CreateAnchor(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const mojom::Pose& native_origin_from_anchor,
      CreateAnchorCallback callback) override;
  void CreatePlaneAttachedAnchor(const mojom::Pose& plane_from_anchor,
                                 uint64_t plane_id,
                                 CreateAnchorCallback callback) override;

  void ProcessAnchorCreationRequests(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state) override;

  void DetachAnchor(uint64_t anchor_id) override;

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

  // ArCore light estimation data
  internal::ScopedArCoreObject<ArLightEstimate*> arcore_light_estimate_;

  // Plane manager. Valid after a call to Initialize.
  std::unique_ptr<ArCorePlaneManager> plane_manager_;
  // Anchor manager. Valid after a call to Initialize.
  std::unique_ptr<ArCoreAnchorManager> anchor_manager_;

  uint64_t next_id_ = 1;

  std::map<HitTestSubscriptionId, HitTestSubscriptionData>
      hit_test_subscription_id_to_data_;
  std::map<HitTestSubscriptionId, TransientInputHitTestSubscriptionData>
      hit_test_subscription_id_to_transient_hit_test_data_;

  std::vector<CreateAnchorRequest> create_anchor_requests_;
  std::vector<CreatePlaneAttachedAnchorRequest>
      create_plane_attached_anchor_requests_;

  HitTestSubscriptionId CreateHitTestSubscriptionId();

  // Returns hit test subscription results for a single subscription given
  // current XRSpace transformation.
  device::mojom::XRHitTestSubscriptionResultDataPtr
  GetHitTestSubscriptionResult(
      HitTestSubscriptionId id,
      const mojom::XRRay& ray,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      const gfx::Transform& ray_transformation);

  // Returns transient hit test subscription results for a single subscription.
  // The results will be grouped by input source as there might be multiple
  // input sources that match input source profile name set on a transient hit
  // test subscription.
  // |input_source_ids_and_transforms| contains tuples with (input source id,
  // mojo from input source transform).
  device::mojom::XRHitTestTransientInputSubscriptionResultDataPtr
  GetTransientHitTestSubscriptionResult(
      HitTestSubscriptionId id,
      const mojom::XRRay& ray,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      const std::vector<std::pair<uint32_t, gfx::Transform>>&
          input_source_ids_and_transforms);

  // Returns true if the given native origin exists, false otherwise.
  bool NativeOriginExists(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);

  // Returns mojo_from_native_origin transform given native origin
  // information. If the transform cannot be found or is unknown, it will return
  // base::nullopt.
  base::Optional<gfx::Transform> GetMojoFromNativeOrigin(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);

  // Returns mojo_from_reference_space transform given reference space
  // category. Mojo_from_reference_space is equivalent to
  // mojo_from_native_origin for native origins that are reference spaces.
  // If the transform cannot be found, it will return base::nullopt.
  base::Optional<gfx::Transform> GetMojoFromReferenceSpace(
      device::mojom::XRReferenceSpaceCategory category,
      const gfx::Transform& mojo_from_viewer);

  // Returns a collection of tuples (input_source_id,
  // mojo_from_input_source) for input sources that match the passed in
  // profile name. Mojo_from_input_source is equivalent to
  // mojo_from_native_origin for native origins that are input sources. If
  // there are no input sources that match the profile name, the result will
  // be empty.
  std::vector<std::pair<uint32_t, gfx::Transform>> GetMojoFromInputSources(
      const std::string& profile_name,
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& maybe_input_state);

  // Processes deferred anchor creation requests.
  // |mojo_from_viewer| - viewer pose in world space of the current frame.
  // |input_state| - current input state.
  // |anchor_creation_requests| - vector of deferred anchor creation requests
  // that are supposed to be processed now; post-call, the vector will only
  // contain the requests that have not been processed.
  // |create_anchor_function| - function to call to actually create the anchor;
  // it will receive the specific anchor creation request, along with position
  // and orientation for the anchor, and must return base::Optional<AnchorId>.
  template <typename T, typename FunctionType>
  void ProcessAnchorCreationRequestsHelper(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state,
      std::vector<T>* anchor_creation_requests,
      FunctionType&& create_anchor_function);

  // Must be last.
  base::WeakPtrFactory<ArCoreImpl> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ArCoreImpl);
};

class ArCoreImplFactory : public ArCoreFactory {
 public:
  std::unique_ptr<ArCore> Create() override;
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_IMPL_H_
