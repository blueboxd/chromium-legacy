// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/touch_input_bindings.h"

#include <string>
#include <utility>

#include "base/base_paths_fuchsia.h"
#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/runners/cast/cast_platform_bindings_ids.h"

namespace {

constexpr char kMessagePortName[] = "cast.__platform__.__touchInput__";
constexpr char kRequestId[] = "requestId";
constexpr char kTouchEnabled[] = "touchEnabled";
constexpr char kDisplayControls[] = "displayControls";

}  // namespace

TouchInputBindings::TouchInputBindings(TouchInputPolicy policy,
                                       fuchsia::web::Frame* frame,
                                       NamedMessagePortConnector* connector)
    : policy_(policy), frame_(frame), connector_(connector) {
  constexpr uint64_t kBindingsId =
      static_cast<uint64_t>(CastPlatformBindingsId::TOUCH_INPUT);

  DCHECK(frame_);
  DCHECK(connector_);

  connector_->Register(kMessagePortName,
                       base::BindRepeating(&TouchInputBindings::OnPortReceived,
                                           base::Unretained(this)));

  base::FilePath bindings_js_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &bindings_js_path));
  bindings_js_path = bindings_js_path.AppendASCII(
      "fuchsia/runners/cast/touch_input_bindings.js");
  frame_->AddBeforeLoadJavaScript(
      kBindingsId, {"*"},
      cr_fuchsia::MemBufferFromFile(base::File(
          bindings_js_path, base::File::FLAG_OPEN | base::File::FLAG_READ)),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response()) << "JavaScript injection error.";
      });
}

TouchInputBindings::~TouchInputBindings() {
  connector_->Unregister(kMessagePortName);
}

void TouchInputBindings::OnPortReceived(
    fidl::InterfaceHandle<fuchsia::web::MessagePort> port) {
  port_ = port.Bind();
  ReadNextMessage();
}

void TouchInputBindings::ReadNextMessage() {
  port_->ReceiveMessage(
      fit::bind_member(this, &TouchInputBindings::OnControlMessageReceived));
}

void TouchInputBindings::OnControlMessageReceived(
    fuchsia::web::WebMessage message) {
  // Parse and validate the contents of |message|.
  std::string message_data;
  if (!cr_fuchsia::StringFromMemBuffer(message.data(), &message_data)) {
    LOG(ERROR) << "Couldn't read payload from control message.";
    port_.Unbind();
    return;
  }

  base::Optional<base::Value> message_parsed;
  message_parsed = base::JSONReader::Read(message_data);
  if (!message_parsed || !message_parsed->is_dict()) {
    LOG(ERROR) << "Invalid control message payload.";
    port_.Unbind();
    return;
  }

  base::Value* request_id_value = message_parsed->FindPath(kRequestId);
  const base::Value* should_enable_touch_value =
      message_parsed->FindPath(kTouchEnabled);
  if (!request_id_value || !should_enable_touch_value) {
    LOG(ERROR) << "Parameter dictionary missing required field(s).";
    port_.Unbind();
    return;
  }

  if (!request_id_value->is_int()) {
    LOG(ERROR) << "requestId is not a number.";
    port_.Unbind();
    return;
  }
  if (!should_enable_touch_value->is_bool()) {
    LOG(ERROR) << "touchEnabled is not a boolean.";
    port_.Unbind();
    return;
  }

  bool should_enable_touch;
  base::Value response(base::Value::Type::DICTIONARY);
  response.SetPath(kRequestId, std::move(*request_id_value));
  switch (policy_) {
    case TouchInputPolicy::FORCE_ENABLE:
      // Touch is always enabled for whitelisted applications, regardless of
      // what they specified in "touchEnabled".
      should_enable_touch = true;

      // Whitelisted applications should not show SDK-provided on-screen
      // controls.
      response.SetPath(kDisplayControls, base::Value(false));
      break;

    case TouchInputPolicy::FORCE_DISABLE:
      // If the application is blacklisted, then reject the Promise by not
      // setting a value in kDisplayControls.
      should_enable_touch = false;
      break;

    case TouchInputPolicy::UNSPECIFIED:
      // If no policy is specified for the application, then apply its requested
      // state.
      should_enable_touch = should_enable_touch_value->GetBool();

      response.SetPath(kDisplayControls, base::Value(true));
      break;
  }

  frame_->SetEnableInput(should_enable_touch);

  // Send the acknowledgement message to JS.
  std::string response_json;
  CHECK(base::JSONWriter::Write(response, &response_json));
  fuchsia::web::WebMessage response_message = {};
  response_message.set_data(cr_fuchsia::MemBufferFromString(
      response_json, "cast-touch-message-response"));
  port_->PostMessage(std::move(response_message),
                     [](fuchsia::web::MessagePort_PostMessage_Result result) {
                       LOG_IF(ERROR, result.is_err())
                           << "PostMessage failed, reason: "
                           << static_cast<int>(result.err());
                     });
  ReadNextMessage();
}
