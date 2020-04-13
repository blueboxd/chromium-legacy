// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/bindings/bindings_manager_cast.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/bindings/grit/resources.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace bindings {

namespace {

const char kNamedMessagePortConnectorBindingsId[] =
    "NAMED_MESSAGE_PORT_CONNECTOR";
const char kControlPortConnectMessage[] = "cast.master.connect";

}  // namespace

BindingsManagerCast::BindingsManagerCast() : cast_web_contents_(nullptr) {
  // NamedMessagePortConnector binding will be injected into page first.
  AddBinding(kNamedMessagePortConnectorBindingsId,
             ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                 IDR_PORT_CONNECTOR_JS));
}

BindingsManagerCast::~BindingsManagerCast() = default;

void BindingsManagerCast::AddBinding(base::StringPiece binding_name,
                                     base::StringPiece binding_script) {
  bindings_by_id_[binding_name.as_string()] = binding_script.as_string();
}

void BindingsManagerCast::AttachToPage(
    chromecast::CastWebContents* cast_web_contents) {
  DCHECK(!cast_web_contents_) << "AttachToPage() was called twice.";
  DCHECK(cast_web_contents);

  cast_web_contents_ = cast_web_contents;
  CastWebContents::Observer::Observe(cast_web_contents_);

  for (const auto& binding : bindings_by_id_) {
    LOG(INFO) << "Register bindings for page. bindingId: " << binding.first;
    cast_web_contents_->AddBeforeLoadJavaScript(
        binding.first /* binding ID */, {"*"}, binding.second /* binding JS */);
  }
}

void BindingsManagerCast::OnPageStateChanged(
    CastWebContents* cast_web_contents) {
  auto page_state = cast_web_contents->page_state();

  switch (page_state) {
    case CastWebContents::PageState::IDLE:
    case CastWebContents::PageState::LOADING:
    case CastWebContents::PageState::CLOSED:
      return;
    case CastWebContents::PageState::DESTROYED:
    case CastWebContents::PageState::ERROR:
      blink_port_.Reset();
      CastWebContents::Observer::Observe(nullptr);
      cast_web_contents_ = nullptr;
      return;
    case CastWebContents::PageState::LOADED:
      OnPageLoaded();
      return;
  }
}

void BindingsManagerCast::OnPageLoaded() {
  DCHECK(cast_web_contents_)
      << "Received PageLoaded event while not observing a page";

  // Unbind platform-side MessagePort connector.
  blink_port_.Reset();

  // Create a blink::WebMessagePort, this is the way Chromium implements HTML5
  // MessagePorts.
  auto port_pair = blink::WebMessagePort::CreatePair();

  blink_port_ = std::move(port_pair.first);
  blink_port_.SetReceiver(this, base::ThreadTaskRunnerHandle::Get());

  // Post the other end of the pipe to the page so that we can receive messages
  // over |content_port|. |named_message_port_connector.js| will receive this
  // through an onmessage event.
  std::vector<blink::WebMessagePort> message_ports;
  message_ports.push_back(std::move(port_pair.second));
  cast_web_contents_->PostMessageToMainFrame("*", kControlPortConnectMessage,
                                             std::move(message_ports));
}

bool BindingsManagerCast::OnMessage(blink::WebMessagePort::Message message) {
  // Receive MessagePort and forward ports to their corresponding
  // binding handlers.

  // One and only one MessagePort should be sent to here.
  if (message.ports.empty())
    LOG(ERROR) << "blink::WebMessagePort::Message contains no ports.";
  DCHECK_EQ(1u, message.ports.size())
      << "Only one control port should be provided";
  blink::WebMessagePort message_port = std::move(message.ports[0]);
  message.ports.clear();

  base::string16 data_utf16 = std::move(message.data);

  std::string binding_id;
  if (!base::UTF16ToUTF8(data_utf16.data(), data_utf16.size(), &binding_id)) {
    return false;
  }

  // Route the port to corresponding binding backend.
  OnPortConnected(binding_id, std::move(message_port));
  return true;
}

void BindingsManagerCast::OnPipeError() {
  LOG(INFO) << "NamedMessagePortConnector control port disconnected";
  blink_port_.Reset();
}

}  // namespace bindings
}  // namespace chromecast
