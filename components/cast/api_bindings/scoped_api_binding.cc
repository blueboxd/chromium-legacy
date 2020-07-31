// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/api_bindings/scoped_api_binding.h"

#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cast/api_bindings/manager.h"

namespace cast_api_bindings {

ScopedApiBinding::ScopedApiBinding(Manager* bindings_manager,
                                   Delegate* delegate,
                                   base::StringPiece js_bindings_id,
                                   base::StringPiece js_bindings)
    : bindings_manager_(bindings_manager),
      delegate_(delegate),
      js_bindings_id_(js_bindings_id) {
  DCHECK(bindings_manager_);
  DCHECK(!js_bindings_id_.empty());

  bindings_manager_->AddBinding(js_bindings_id_, js_bindings);

  if (delegate_) {
    bindings_manager_->RegisterPortHandler(
        delegate_->GetPortName(),
        base::BindRepeating(&ScopedApiBinding::OnPortConnected,
                            base::Unretained(this)));
  }
}

ScopedApiBinding::~ScopedApiBinding() {
  // TODO(crbug.com/1104369): Remove binding JS when RemoveBinding() added to
  // ApiBindingsManager.

  if (delegate_) {
    bindings_manager_->UnregisterPortHandler(delegate_->GetPortName());
  }
}

void ScopedApiBinding::OnPortConnected(blink::WebMessagePort port) {
  message_port_ = std::move(port);
  message_port_.SetReceiver(this, base::SequencedTaskRunnerHandle::Get());
  delegate_->OnConnected();
}

bool ScopedApiBinding::SendMessage(base::StringPiece data_utf8) {
  DCHECK(delegate_);

  DVLOG(1) << "SendMessage: message=" << data_utf8;
  if (!message_port_.IsValid()) {
    LOG(WARNING)
        << "Attempted to write to unconnected MessagePort, dropping message.";
    return false;
  }

  if (!message_port_.PostMessage(
          blink::WebMessagePort::Message(base::UTF8ToUTF16(data_utf8)))) {
    return false;
  }

  return true;
}

bool ScopedApiBinding::OnMessage(blink::WebMessagePort::Message message) {
  std::string message_utf8;
  if (!base::UTF16ToUTF8(message.data.data(), message.data.size(),
                         &message_utf8)) {
    return false;
  }

  return delegate_->OnMessage(message_utf8);
}

void ScopedApiBinding::OnPipeError() {
  delegate_->OnDisconnected();
}

}  // namespace cast_api_bindings
