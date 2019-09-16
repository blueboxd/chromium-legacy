// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/test/openxr_test_helper.h"

#include <cmath>
#include <limits>

#include "device/vr/openxr/openxr_util.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"
#include "third_party/openxr/src/src/common/hex_and_handles.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace {
bool PathContainsString(const std::string& path, const std::string& s) {
  return path.find(s) != std::string::npos;
}

}  // namespace

// Initialize static variables in OpenXrTestHelper.
const char* OpenXrTestHelper::kExtensions[] = {
    XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
const uint32_t OpenXrTestHelper::kDimension = 128;
const uint32_t OpenXrTestHelper::kSwapCount = 1;
const uint32_t OpenXrTestHelper::kMinSwapchainBuffering = 3;
const uint32_t OpenXrTestHelper::kMaxViewCount = 2;
const XrViewConfigurationView OpenXrTestHelper::kViewConfigView = {
    XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr,
    OpenXrTestHelper::kDimension,    OpenXrTestHelper::kDimension,
    OpenXrTestHelper::kDimension,    OpenXrTestHelper::kDimension,
    OpenXrTestHelper::kSwapCount,    OpenXrTestHelper::kSwapCount};
XrViewConfigurationView OpenXrTestHelper::kViewConfigurationViews[] = {
    OpenXrTestHelper::kViewConfigView, OpenXrTestHelper::kViewConfigView};
const XrViewConfigurationType OpenXrTestHelper::kViewConfigurationType =
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
const XrEnvironmentBlendMode OpenXrTestHelper::kEnvironmentBlendMode =
    XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

uint32_t OpenXrTestHelper::NumExtensionsSupported() {
  return sizeof(kExtensions) / sizeof(kExtensions[0]);
}

uint32_t OpenXrTestHelper::NumViews() {
  return sizeof(kViewConfigurationViews) / sizeof(kViewConfigurationViews[0]);
}

OpenXrTestHelper::OpenXrTestHelper()
    : system_id_(0),
      session_(XR_NULL_HANDLE),
      session_state_(XR_SESSION_STATE_UNKNOWN),
      swapchain_(XR_NULL_HANDLE),
      local_space_(XR_NULL_HANDLE),
      view_space_(XR_NULL_HANDLE),
      acquired_swapchain_texture_(0),
      next_action_space_(0),
      next_predicted_display_time_(0) {}

OpenXrTestHelper::~OpenXrTestHelper() = default;

void OpenXrTestHelper::TestFailure() {
  NOTREACHED();
}

void OpenXrTestHelper::SetTestHook(device::VRTestHook* hook) {
  base::AutoLock auto_lock(lock_);
  test_hook_ = hook;
}

void OpenXrTestHelper::OnPresentedFrame() {
  static uint32_t frame_id = 1;

  base::AutoLock auto_lock(lock_);
  if (!test_hook_)
    return;

  // TODO(https://crbug.com/986621): The frame color is currently hard-coded to
  // what the pixel tests expects. We should instead store the actual WebGL
  // texture and read from it, which will also verify the correct swapchain
  // texture was used.

  device::DeviceConfig device_config = test_hook_->WaitGetDeviceConfig();
  device::SubmittedFrameData frame_data = {};

  if (std::abs(device_config.interpupillary_distance - 0.2f) <
      std::numeric_limits<float>::epsilon()) {
    // TestPresentationPoses sets the ipd to 0.2f, whereas tests by default have
    // an ipd of 0.1f. This test has specific formulas to determine the colors,
    // specified in test_webxr_poses.html.
    frame_data.color = {
        frame_id % 256, ((frame_id - frame_id % 256) / 256) % 256,
        ((frame_id - frame_id % (256 * 256)) / (256 * 256)) % 256, 255};
  } else {
    // The WebXR tests by default clears to blue. TestPresentationPixels
    // verifies this color.
    frame_data.color = {0, 0, 255, 255};
  }

  frame_data.left_eye = true;
  test_hook_->OnFrameSubmitted(frame_data);

  frame_data.left_eye = false;
  test_hook_->OnFrameSubmitted(frame_data);

  frame_id++;
}

XrSystemId OpenXrTestHelper::GetSystemId() {
  system_id_ = 1;
  return system_id_;
}

XrResult OpenXrTestHelper::GetSession(XrSession* session) {
  RETURN_IF(session_state_ != XR_SESSION_STATE_UNKNOWN,
            XR_ERROR_VALIDATION_FAILURE,
            "SessionState is not unknown before xrCreateSession");
  session_ = TreatIntegerAsHandle<XrSession>(2);
  *session = session_;
  SetSessionState(XR_SESSION_STATE_READY);
  return XR_SUCCESS;
}

XrSwapchain OpenXrTestHelper::GetSwapchain() {
  swapchain_ = TreatIntegerAsHandle<XrSwapchain>(3);
  return swapchain_;
}

XrResult OpenXrTestHelper::GetActionStateBoolean(
    XrAction action,
    XrActionStateBoolean* data) const {
  XrResult xr_result;

  RETURN_IF_XR_FAILED(ValidateAction(action));
  const ActionProperties& cur_action_properties = actions_.at(action);
  RETURN_IF(cur_action_properties.type != XR_ACTION_TYPE_BOOLEAN_INPUT,
            XR_ERROR_ACTION_TYPE_MISMATCH,
            "GetActionStateBoolean type mismatch");
  *data = boolean_action_states_.at(action);
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::GetActionStateVector2f(
    XrAction action,
    XrActionStateVector2f* data) const {
  XrResult xr_result;

  RETURN_IF_XR_FAILED(ValidateAction(action));
  const ActionProperties& cur_action_properties = actions_.at(action);
  RETURN_IF(cur_action_properties.type != XR_ACTION_TYPE_VECTOR2F_INPUT,
            XR_ERROR_ACTION_TYPE_MISMATCH,
            "GetActionStateVector2f type mismatch");
  *data = v2f_action_states_.at(action);
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::GetActionStatePose(XrAction action,
                                              XrActionStatePose* data) const {
  XrResult xr_result;

  RETURN_IF_XR_FAILED(ValidateAction(action));
  const ActionProperties& cur_action_properties = actions_.at(action);
  RETURN_IF(cur_action_properties.type != XR_ACTION_TYPE_POSE_INPUT,
            XR_ERROR_ACTION_TYPE_MISMATCH,
            "GetActionStateVector2f type mismatch");
  *data = pose_action_state_.at(action);
  return XR_SUCCESS;
}

XrSpace OpenXrTestHelper::CreateLocalSpace() {
  local_space_ = TreatIntegerAsHandle<XrSpace>(++next_action_space_);
  return local_space_;
}

XrSpace OpenXrTestHelper::CreateViewSpace() {
  view_space_ = TreatIntegerAsHandle<XrSpace>(++next_action_space_);
  return view_space_;
}

XrAction OpenXrTestHelper::CreateAction(XrActionSet action_set,
                                        const XrActionCreateInfo& create_info) {
  action_names_.emplace(create_info.actionName);
  action_localized_names_.emplace(create_info.localizedActionName);
  // The OpenXR Loader will return an error if the action handle is 0.
  XrAction cur_action = TreatIntegerAsHandle<XrAction>(actions_.size() + 1);
  ActionProperties cur_action_properties;
  cur_action_properties.type = create_info.actionType;
  switch (create_info.actionType) {
    case XR_ACTION_TYPE_BOOLEAN_INPUT: {
      boolean_action_states_[cur_action];
      break;
    }
    case XR_ACTION_TYPE_VECTOR2F_INPUT: {
      v2f_action_states_[cur_action];
      break;
    }
    case XR_ACTION_TYPE_POSE_INPUT: {
      pose_action_state_[cur_action];
      break;
    }
    default: {
      LOG(ERROR)
          << __FUNCTION__
          << "This type of Action is not supported by test at the moment";
    }
  }

  action_sets_[action_set].push_back(cur_action);
  actions_[cur_action] = cur_action_properties;
  return cur_action;
}

XrActionSet OpenXrTestHelper::CreateActionSet(
    const XrActionSetCreateInfo& createInfo) {
  action_set_names_.emplace(createInfo.actionSetName);
  action_set_localized_names_.emplace(createInfo.localizedActionSetName);
  // The OpenXR Loader will return an error if the action set handle is 0.
  XrActionSet cur_action_set =
      TreatIntegerAsHandle<XrActionSet>(action_sets_.size() + 1);
  action_sets_[cur_action_set];
  return cur_action_set;
}

XrSpace OpenXrTestHelper::CreateActionSpace() {
  return TreatIntegerAsHandle<XrSpace>(++next_action_space_);
}

XrPath OpenXrTestHelper::GetPath(const char* path_string) {
  for (auto it = paths_.begin(); it != paths_.end(); it++) {
    if (it->compare(path_string) == 0) {
      return it - paths_.begin();
    }
  }
  paths_.emplace_back(path_string);
  return paths_.size() - 1;
}

XrResult OpenXrTestHelper::BeginSession() {
  RETURN_IF(session_state_ != XR_SESSION_STATE_READY,
            XR_ERROR_VALIDATION_FAILURE,
            "Session is not XR_ERROR_SESSION_NOT_READY");
  SetSessionState(XR_SESSION_STATE_SYNCHRONIZED);
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::EndSession() {
  RETURN_IF(session_state_ != XR_SESSION_STATE_STOPPING,
            XR_ERROR_VALIDATION_FAILURE,
            "Session state is not XR_ERROR_SESSION_NOT_STOPPING");
  SetSessionState(XR_SESSION_STATE_IDLE);
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::BindActionAndPath(XrActionSuggestedBinding binding) {
  ActionProperties& current_action = actions_[binding.action];
  RETURN_IF(current_action.binding != XR_NULL_PATH, XR_ERROR_VALIDATION_FAILURE,
            "BindActionAndPath action is bind to more than one path, this is "
            "not cupported with current test");
  current_action.binding = binding.binding;
  return XR_SUCCESS;
}

void OpenXrTestHelper::SetD3DDevice(ID3D11Device* d3d_device) {
  DCHECK(d3d_device_ == nullptr);
  DCHECK(d3d_device != nullptr);
  d3d_device_ = d3d_device;

  D3D11_TEXTURE2D_DESC desc{};
  desc.Width = kDimension * 2;  // Using a double wide texture
  desc.Height = kDimension;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET;

  for (uint32_t i = 0; i < kMinSwapchainBuffering; i++) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = d3d_device_->CreateTexture2D(&desc, nullptr, &texture);
    DCHECK(hr == S_OK);

    textures_arr_.push_back(texture);
  }
}

XrResult OpenXrTestHelper::SyncActionData(XrActionSet action_set) {
  XrResult xr_result;

  RETURN_IF_XR_FAILED(ValidateActionSet(action_set));
  const std::vector<XrAction>& actions = action_sets_[action_set];
  for (uint32_t i = 0; i < actions.size(); i++) {
    RETURN_IF_XR_FAILED(UpdateAction(actions[i]));
  }
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::UpdateAction(XrAction action) {
  XrResult xr_result;
  RETURN_IF_XR_FAILED(ValidateAction(action));
  const ActionProperties& cur_action_properties = actions_[action];
  std::string path_string = PathToString(cur_action_properties.binding);
  bool support_path =
      PathContainsString(path_string, "/user/hand/left/input") ||
      PathContainsString(path_string, "/user/hand/right/input");
  RETURN_IF_FALSE(
      support_path, XR_ERROR_VALIDATION_FAILURE,
      "UpdateAction this action has a path that is not supported by test now");
  device::ControllerFrameData data;
  device::ControllerRole role;
  if (PathContainsString(path_string, "/user/hand/left/")) {
    role = device::kControllerRoleLeft;
  } else if (PathContainsString(path_string, "/user/hand/right/")) {
    role = device::kControllerRoleRight;
  } else {
    LOG(ERROR) << "Currently Action should belong to either left or right";
    NOTREACHED();
  }
  for (uint32_t i = 0; i < data_arr_.size(); i++) {
    if (data_arr_[i].role == role) {
      data = data_arr_[i];
    }
  }

  switch (cur_action_properties.type) {
    case XR_ACTION_TYPE_BOOLEAN_INPUT: {
      device::XrButtonId button_id = device::kMax;
      if (PathContainsString(path_string, "/trackpad/")) {
        button_id = device::kAxisTrackpad;
      } else if (PathContainsString(path_string, "/trigger/")) {
        button_id = device::kAxisTrigger;
      } else if (PathContainsString(path_string, "/squeeze/")) {
        button_id = device::kGrip;
      } else if (PathContainsString(path_string, "/menu/")) {
        button_id = device::kMenu;
      } else {
        NOTREACHED() << "Curently test does not support this button";
      }
      uint64_t button_mask = XrButtonMaskFromId(button_id);

      // This bool pressed is needed because XrActionStateBoolean.currentState
      // is XrBool32 which is uint32_t. And it won't behave correctly if we try
      // to set is using uint64_t value like button_mask. like the following:
      // boolean_action_states_[].currentState = data.buttons_pressed &
      // button_mask
      bool pressed = data.buttons_pressed & button_mask;
      boolean_action_states_[action].currentState = pressed;
      boolean_action_states_[action].isActive = data.is_valid;
      break;
    }
    case XR_ACTION_TYPE_VECTOR2F_INPUT: {
      // index is the same as the sequence action is created they should be
      // consistent when set in the tests
      v2f_action_states_[action].isActive = data.is_valid;
      break;
    }
    case XR_ACTION_TYPE_POSE_INPUT: {
      pose_action_state_[action].isActive = data.is_valid;
      break;
    }
    default: {
      RETURN_IF_FALSE(false, XR_ERROR_VALIDATION_FAILURE,
                      "UpdateAction does not support this type of action");
      break;
    }
  }

  return XR_SUCCESS;
}

void OpenXrTestHelper::SetSessionState(XrSessionState state) {
  session_state_ = state;
  XrEventDataSessionStateChanged event = {
      XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};
  event.session = session_;
  event.state = session_state_;
  event.time = next_predicted_display_time_;
  session_state_event_queue_.push(event);
}

XrEventDataSessionStateChanged OpenXrTestHelper::GetNextSessionStateEvent() {
  DCHECK(HasPendingSessionStateEvent());
  XrEventDataSessionStateChanged front = session_state_event_queue_.front();
  session_state_event_queue_.pop();
  return front;
}

const std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>>&
OpenXrTestHelper::GetSwapchainTextures() const {
  return textures_arr_;
}

uint32_t OpenXrTestHelper::NextSwapchainImageIndex() {
  acquired_swapchain_texture_ =
      (acquired_swapchain_texture_ + 1) % textures_arr_.size();
  return acquired_swapchain_texture_;
}

XrTime OpenXrTestHelper::NextPredictedDisplayTime() {
  return ++next_predicted_display_time_;
}

bool OpenXrTestHelper::UpdateSessionStateEventQueue() {
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    if (test_hook_->WaitGetSessionStateStopping()) {
      SetSessionState(XR_SESSION_STATE_STOPPING);
    }
    return true;
  }

  return false;
}

bool OpenXrTestHelper::HasPendingSessionStateEvent() {
  return !session_state_event_queue_.empty();
}

void OpenXrTestHelper::GetPose(XrPosef* pose) {
  *pose = device::PoseIdentity();

  base::AutoLock lock(lock_);
  if (test_hook_) {
    device::PoseFrameData pose_data = test_hook_->WaitGetPresentingPose();
    if (pose_data.is_valid) {
      gfx::Transform transform = PoseFrameDataToTransform(pose_data);

      gfx::DecomposedTransform decomposed_transform;
      bool decomposable =
          gfx::DecomposeTransform(&decomposed_transform, transform);
      DCHECK(decomposable);

      pose->orientation.x = decomposed_transform.quaternion.x();
      pose->orientation.y = decomposed_transform.quaternion.y();
      pose->orientation.z = decomposed_transform.quaternion.z();
      pose->orientation.w = decomposed_transform.quaternion.w();

      pose->position.x = decomposed_transform.translate[0];
      pose->position.y = decomposed_transform.translate[1];
      pose->position.z = decomposed_transform.translate[2];
    }
  }
}

std::string OpenXrTestHelper::PathToString(XrPath path) const {
  return paths_[path];
}

bool OpenXrTestHelper::UpdateData() {
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    for (uint32_t i = 0; i < device::kMaxTrackedDevices; i++) {
      data_arr_[i] = test_hook_->WaitGetControllerData(i);
    }
    return true;
  }
  return false;
}

bool OpenXrTestHelper::UpdateViewFOV(XrView views[], uint32_t size) {
  RETURN_IF(size != 2, XR_ERROR_VALIDATION_FAILURE,
            "UpdateViewFOV currently only supports 2 viewports config");
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    auto config = test_hook_->WaitGetDeviceConfig();
    views[0].pose.position.x = config.interpupillary_distance / 2;
    views[1].pose.position.x = -config.interpupillary_distance / 2;
  }
  return true;
}

XrResult OpenXrTestHelper::ValidateAction(XrAction action) const {
  RETURN_IF(actions_.count(action) != 1, XR_ERROR_HANDLE_INVALID,
            "ValidateAction: Invalid Action");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateActionCreateInfo(
    const XrActionCreateInfo& create_info) const {
  RETURN_IF(create_info.type != XR_TYPE_ACTION_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionCreateInfo type invalid");
  RETURN_IF(create_info.actionName[0] == '\0', XR_ERROR_NAME_INVALID,
            "ValidateActionCreateInfo actionName invalid");
  RETURN_IF(create_info.localizedActionName[0] == '\0', XR_ERROR_NAME_INVALID,
            "ValidateActionCreateInfo localizedActionName invalid");
  RETURN_IF(action_names_.count(create_info.actionName) != 0,
            XR_ERROR_NAME_DUPLICATED,
            "ValidateActionCreateInfo actionName duplicate");
  RETURN_IF(action_localized_names_.count(create_info.localizedActionName) != 0,
            XR_ERROR_NAME_DUPLICATED,
            "ValidateActionCreateInfo localizedActionName duplicate");
  RETURN_IF_FALSE(create_info.countSubactionPaths == 0 &&
                      create_info.subactionPaths == nullptr,
                  XR_ERROR_VALIDATION_FAILURE,
                  "ValidateActionCreateInfo has subactionPaths which is not "
                  "supported by current version of test.");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateActionSet(XrActionSet action_set) const {
  RETURN_IF_FALSE(action_sets_.count(action_set), XR_ERROR_HANDLE_INVALID,
                  "ValidateActionSet: Invalid action_set");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateActionSetCreateInfo(
    const XrActionSetCreateInfo& create_info) const {
  RETURN_IF(create_info.type != XR_TYPE_ACTION_SET_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionSetCreateInfo type invalid");
  RETURN_IF(create_info.actionSetName[0] == '\0', XR_ERROR_NAME_INVALID,
            "ValidateActionSetCreateInfo actionSetName invalid");
  RETURN_IF(create_info.localizedActionSetName[0] == '\0',
            XR_ERROR_NAME_INVALID,
            "ValidateActionSetCreateInfo localizedActionSetName invalid");
  RETURN_IF(action_set_names_.count(create_info.actionSetName) != 0,
            XR_ERROR_NAME_DUPLICATED,
            "ValidateActionSetCreateInfo actionSetName duplicate");
  RETURN_IF(action_set_localized_names_.count(
                create_info.localizedActionSetName) != 0,
            XR_ERROR_NAME_DUPLICATED,
            "ValidateActionSetCreateInfo localizedActionSetName duplicate");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateActionSpaceCreateInfo(
    const XrActionSpaceCreateInfo& create_info) const {
  XrResult xr_result;
  RETURN_IF(create_info.type != XR_TYPE_ACTION_SPACE_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionSpaceCreateInfo type invalid");
  RETURN_IF_XR_FAILED(ValidateAction(create_info.action));
  RETURN_IF(create_info.subactionPath != XR_NULL_PATH,
            XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionSpaceCreateInfo subactionPath != XR_NULL_PATH");
  RETURN_IF_XR_FAILED(ValidateXrPosefIsIdentity(create_info.poseInActionSpace));
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateInstance(XrInstance instance) const {
  // The Fake OpenXr Runtime returns this global OpenXrTestHelper object as the
  // instance value on xrCreateInstance.
  RETURN_IF(reinterpret_cast<OpenXrTestHelper*>(instance) != this,
            XR_ERROR_VALIDATION_FAILURE, "XrInstance invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSystemId(XrSystemId system_id) const {
  RETURN_IF(system_id_ == 0, XR_ERROR_SYSTEM_INVALID,
            "XrSystemId has not been queried");
  RETURN_IF(system_id != system_id_, XR_ERROR_SYSTEM_INVALID,
            "XrSystemId invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSession(XrSession session) const {
  RETURN_IF(session_ == XR_NULL_HANDLE, XR_ERROR_VALIDATION_FAILURE,
            "XrSession has not been queried");
  RETURN_IF(session != session_, XR_ERROR_VALIDATION_FAILURE,
            "XrSession invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSwapchain(XrSwapchain swapchain) const {
  RETURN_IF(swapchain_ == XR_NULL_HANDLE, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchain has not been queried");
  RETURN_IF(swapchain != swapchain_, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchain invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSpace(XrSpace space) const {
  RETURN_IF(space == XR_NULL_HANDLE, XR_ERROR_HANDLE_INVALID,
            "XrSpace has not been queried");
  RETURN_IF(MakeHandleGeneric(space) > next_action_space_,
            XR_ERROR_HANDLE_INVALID, "XrSpace invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidatePath(XrPath path) const {
  RETURN_IF(path >= paths_.size(), XR_ERROR_PATH_INVALID, "XrPath invalid");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidatePredictedDisplayTime(XrTime time) const {
  RETURN_IF(time == 0, XR_ERROR_VALIDATION_FAILURE,
            "XrTime has not been queried");
  RETURN_IF(time > next_predicted_display_time_, XR_ERROR_VALIDATION_FAILURE,
            "XrTime predicted display time invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateXrPosefIsIdentity(
    const XrPosef& pose) const {
  XrPosef identity = device::PoseIdentity();
  bool is_identity = true;
  is_identity &= pose.orientation.x == identity.orientation.x;
  is_identity &= pose.orientation.y == identity.orientation.y;
  is_identity &= pose.orientation.z == identity.orientation.z;
  is_identity &= pose.orientation.w == identity.orientation.w;
  is_identity &= pose.position.x == identity.position.x;
  is_identity &= pose.position.y == identity.position.y;
  is_identity &= pose.position.z == identity.position.z;
  RETURN_IF_FALSE(is_identity, XR_ERROR_VALIDATION_FAILURE,
                  "XrPosef is not an identity");

  return XR_SUCCESS;
}
