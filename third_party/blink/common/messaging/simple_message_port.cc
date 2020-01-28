// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/simple_message_port.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/messaging/transferable_message_mojom_traits.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"

namespace blink {

SimpleMessagePort::Message::Message() = default;
SimpleMessagePort::Message::Message(Message&&) = default;
SimpleMessagePort::Message& SimpleMessagePort::Message::operator=(Message&&) =
    default;
SimpleMessagePort::Message::~Message() = default;

SimpleMessagePort::Message::Message(const base::string16& data) : data(data) {}

SimpleMessagePort::Message::Message(std::vector<SimpleMessagePort> ports)
    : ports(std::move(ports)) {}

SimpleMessagePort::Message::Message(SimpleMessagePort&& port) {
  ports.emplace_back(std::move(port));
}

SimpleMessagePort::Message::Message(const base::string16& data,
                                    std::vector<SimpleMessagePort> ports)
    : data(data), ports(std::move(ports)) {}

SimpleMessagePort::Message::Message(const base::string16& data,
                                    SimpleMessagePort port)
    : data(data) {
  ports.emplace_back(std::move(port));
}

SimpleMessagePort::MessageReceiver::MessageReceiver() = default;
SimpleMessagePort::MessageReceiver::~MessageReceiver() = default;
bool SimpleMessagePort::MessageReceiver::OnMessage(Message) {
  return true;
}

SimpleMessagePort::SimpleMessagePort() = default;

SimpleMessagePort::SimpleMessagePort(SimpleMessagePort&& other) {
  Take(std::move(other));
}

SimpleMessagePort& SimpleMessagePort::operator=(SimpleMessagePort&& other) {
  Take(std::move(other));
  return *this;
}

SimpleMessagePort::~SimpleMessagePort() {
  CloseIfNecessary();
}

// static
std::pair<SimpleMessagePort, SimpleMessagePort>
SimpleMessagePort::CreatePair() {
  mojo::ScopedMessagePipeHandle handle0, handle1;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateMessagePipe(nullptr, &handle0, &handle1));
  return std::make_pair(SimpleMessagePort(std::move(handle0)),
                        SimpleMessagePort(std::move(handle1)));
}

void SimpleMessagePort::SetReceiver(
    MessageReceiver* receiver,
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
      base::BindOnce(&SimpleMessagePort::OnPipeError, base::Unretained(this)));
}

void SimpleMessagePort::ClearReceiver() {
  if (!connector_)
    return;
  port_ = connector_->PassMessagePipe();
  connector_.reset();
  receiver_ = nullptr;
}

base::SequencedTaskRunner* SimpleMessagePort::GetTaskRunner() const {
  if (!connector_)
    return nullptr;
  return connector_->task_runner();
}

mojo::ScopedMessagePipeHandle SimpleMessagePort::PassHandle() {
  DCHECK(is_transferable_);

  // Clear the receiver, which takes the handle out of the connector if it
  // exists, and puts it back in |port_|.
  ClearReceiver();
  mojo::ScopedMessagePipeHandle handle = std::move(port_);
  Reset();
  return handle;
}

SimpleMessagePort::SimpleMessagePort(mojo::ScopedMessagePipeHandle&& port)
    : port_(std::move(port)), is_closed_(false), is_transferable_(true) {
  DCHECK(port_.is_valid());
}

bool SimpleMessagePort::CanPostMessage() const {
  return connector_ && connector_->is_valid() && !is_closed_ && !is_errored_ &&
         receiver_;
}

bool SimpleMessagePort::PostMessage(Message&& message) {
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

bool SimpleMessagePort::IsValid() const {
  if (connector_)
    return connector_->is_valid();
  return port_.is_valid();
}

void SimpleMessagePort::Close() {
  CloseIfNecessary();
}

void SimpleMessagePort::Reset() {
  CloseIfNecessary();
  is_closed_ = true;
  is_errored_ = false;
  is_transferable_ = false;
}

void SimpleMessagePort::Take(SimpleMessagePort&& other) {
  port_ = std::move(other.port_);
  connector_ = std::move(other.connector_);
  is_closed_ = std::exchange(other.is_closed_, true);
  is_errored_ = std::exchange(other.is_errored_, false);
  is_transferable_ = std::exchange(other.is_transferable_, false);
  receiver_ = std::exchange(other.receiver_, nullptr);
}

void SimpleMessagePort::OnPipeError() {
  DCHECK(!is_transferable_);
  if (is_errored_)
    return;
  is_errored_ = true;
  if (receiver_)
    receiver_->OnPipeError();
}

void SimpleMessagePort::CloseIfNecessary() {
  if (is_closed_)
    return;
  is_closed_ = true;
  ClearReceiver();
  port_.reset();
}

bool SimpleMessagePort::Accept(mojo::Message* mojo_message) {
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
    message.ports.emplace_back(SimpleMessagePort(std::move(handle)));
  }

  // Pass the message on to the receiver.
  return receiver_->OnMessage(std::move(message));
}

}  // namespace blink
