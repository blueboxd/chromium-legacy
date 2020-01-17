// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/message_port.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/messaging/transferable_message_mojom_traits.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"

namespace content {

MessagePort::Message::Message() = default;
MessagePort::Message::Message(Message&&) = default;
MessagePort::Message& MessagePort::Message::operator=(Message&&) = default;
MessagePort::Message::~Message() = default;

MessagePort::Message::Message(const base::string16& data) : data(data) {}

MessagePort::Message::Message(std::vector<MessagePort> ports)
    : ports(std::move(ports)) {}

MessagePort::Message::Message(MessagePort&& port) {
  ports.emplace_back(std::move(port));
}

MessagePort::Message::Message(const base::string16& data,
                              std::vector<MessagePort> ports)
    : data(data), ports(std::move(ports)) {}

MessagePort::Message::Message(const base::string16& data, MessagePort port)
    : data(data) {
  ports.emplace_back(std::move(port));
}

MessagePort::MessageReceiver::MessageReceiver() = default;
MessagePort::MessageReceiver::~MessageReceiver() = default;
bool MessagePort::MessageReceiver::OnMessage(Message) {
  return true;
}

MessagePort::MessagePort() = default;

MessagePort::MessagePort(MessagePort&& other) {
  Take(std::move(other));
}

MessagePort& MessagePort::operator=(MessagePort&& other) {
  Take(std::move(other));
  return *this;
}

MessagePort::~MessagePort() {
  CloseIfNecessary();
}

// static
std::pair<MessagePort, MessagePort> MessagePort::CreatePair() {
  mojo::ScopedMessagePipeHandle handle0, handle1;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateMessagePipe(nullptr, &handle0, &handle1));
  return std::make_pair(MessagePort(std::move(handle0)),
                        MessagePort(std::move(handle1)));
}

void MessagePort::SetReceiver(MessageReceiver* receiver,
                              scoped_refptr<base::SequencedTaskRunner> runner) {
  DCHECK(receiver);
  DCHECK(runner.get());

  DCHECK(port_.is_valid());
  DCHECK(!connector_);
  DCHECK(!is_closed_);
  DCHECK(!is_errored_);
  DCHECK(is_transferable_);

  is_transferable_ = false;
  receiver_ = receiver;
  connector_ = std::make_unique<mojo::Connector>(
      std::move(port_), mojo::Connector::SINGLE_THREADED_SEND,
      std::move(runner));
  connector_->set_incoming_receiver(this);
  connector_->set_connection_error_handler(
      base::BindOnce(&MessagePort::OnPipeError, base::Unretained(this)));
}

void MessagePort::ClearReceiver() {
  if (!connector_)
    return;
  port_ = connector_->PassMessagePipe();
  connector_.reset();
  receiver_ = nullptr;
}

base::SequencedTaskRunner* MessagePort::GetTaskRunner() const {
  if (!connector_)
    return nullptr;
  return connector_->task_runner();
}

mojo::ScopedMessagePipeHandle MessagePort::PassHandle() {
  DCHECK(is_transferable_);

  // Clear the receiver, which takes the handle out of the connector if it
  // exists, and puts it back in |port_|.
  ClearReceiver();
  mojo::ScopedMessagePipeHandle handle = std::move(port_);
  Reset();
  return handle;
}

MessagePort::MessagePort(mojo::ScopedMessagePipeHandle&& port)
    : port_(std::move(port)), is_closed_(false), is_transferable_(true) {
  DCHECK(port_.is_valid());
}

bool MessagePort::CanPostMessage() const {
  return connector_ && connector_->is_valid() && !is_closed_ && !is_errored_ &&
         receiver_;
}

bool MessagePort::PostMessage(Message&& message) {
  if (!CanPostMessage())
    return false;

  // Extract the underlying handles for transport in a
  // blink::TransferableMessage.
  std::vector<mojo::ScopedMessagePipeHandle> handles;
  for (auto& port : message.ports) {
    // We should not be trying to send ourselves in a message. Mojo prevents
    // this at a deeper level, but we can also check here.
    DCHECK_NE(this, &port);

    handles.emplace_back(port.PassHandle());
  }

  // Build the message.
  // TODO(chrisha): Finally kill off MessagePortChannel, once
  // MessagePortDescriptor more thoroughly plays that role.
  blink::TransferableMessage transferable_message;
  transferable_message.owned_encoded_message =
      blink::EncodeStringMessage(message.data);
  transferable_message.encoded_message =
      transferable_message.owned_encoded_message;
  transferable_message.ports =
      blink::MessagePortChannel::CreateFromHandles(std::move(handles));

  // TODO(chrisha): Notify the instrumentation delegate of a message being sent!

  // Send via Mojo. The message should never be malformed so should always be
  // accepted.
  mojo::Message mojo_message =
      blink::mojom::TransferableMessage::SerializeAsMessage(
          &transferable_message);
  CHECK(connector_->Accept(&mojo_message));

  return true;
}

bool MessagePort::IsValid() const {
  if (connector_)
    return connector_->is_valid();
  return port_.is_valid();
}

void MessagePort::Close() {
  CloseIfNecessary();
}

void MessagePort::Reset() {
  CloseIfNecessary();
  is_closed_ = true;
  is_errored_ = false;
  is_transferable_ = false;
}

void MessagePort::Take(MessagePort&& other) {
  port_ = std::move(other.port_);
  connector_ = std::move(other.connector_);
  is_closed_ = std::exchange(other.is_closed_, true);
  is_errored_ = std::exchange(other.is_errored_, false);
  is_transferable_ = std::exchange(other.is_transferable_, false);
  receiver_ = std::exchange(other.receiver_, nullptr);
}

void MessagePort::OnPipeError() {
  DCHECK(!is_transferable_);
  if (is_errored_)
    return;
  is_errored_ = true;
  if (receiver_)
    receiver_->OnPipeError();
}

void MessagePort::CloseIfNecessary() {
  if (is_closed_)
    return;
  is_closed_ = true;
  ClearReceiver();
  port_.reset();
}

bool MessagePort::Accept(mojo::Message* mojo_message) {
  DCHECK(receiver_);
  DCHECK(!is_transferable_);

  // Deserialize the message.
  blink::TransferableMessage transferable_message;
  if (!blink::mojom::TransferableMessage::DeserializeFromMessage(
          std::move(*mojo_message), &transferable_message)) {
    return false;
  }

  // Decode the string portion of the message.
  Message message;
  if (!blink::DecodeStringMessage(transferable_message.encoded_message,
                                  &message.data)) {
    return false;
  }

  // Convert raw handles to MessagePorts.
  // TODO(chrisha): Kill off MessagePortChannel entirely!
  auto handles =
      blink::MessagePortChannel::ReleaseHandles(transferable_message.ports);
  for (auto& handle : handles) {
    message.ports.emplace_back(MessagePort(std::move(handle)));
  }

  // Pass the message on to the receiver.
  return receiver_->OnMessage(std::move(message));
}

}  // namespace content
