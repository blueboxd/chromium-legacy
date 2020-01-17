// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_H_
#define CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_H_

#include <utility>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {

// Defines a MessagePort, which is used for sending and receiving messages to
// Javascript content. This is a more limited version of blink::MessagePort
// that is intended for use by embedders. It is a lightweight wrapper of
// a Mojo message pipe, and provides functionality for sending and
// receiving messages, automatically handling the serialization. It is
// analogous to the Java org.chromium.content_public.browser.MessagePort.
//
// Intended embedder usage is as follows:
//
//  // Create a pair of ports. The two ends of the pipe are conjugates of each
//  // other.
//  std::pair<MessagePort, MessagePort> ports = MessagePort::CreatePair();
//
//  // Keep one end for ourselves.
//  MessageReceiverImpl receiver;  // Implements MessagePort::MessageReceiver.
//  auto embedder_port = std::move(ports.first);
//  embedder_port.SetReceiver(&receiver, task_runner);
//
//  // Send the other end of the pipe to a WebContents. This will arrive in the
//  // main frame of that WebContents.
//  std::vector<mojo::ScopedMessagePipeHandle> handles;
//  handles.emplace_back(ports.second.PassHandle());
//  MessagePortProvider::PostMessageToFrame(
//      web_contents, ..., std::move(handles));
//
//  // The web contents can now talk back to us via |embedder_port|, and we can
//  // talk back directly to it over that same pipe rather than via the
//  // MessagePortProvider API.
//
// Note that some embedders provide "PostMessageToFrame" functions directly on
// their wrapped WebContents equivalents (Android and Cast for example). Also
// note that for Android embedders, there are equivalent Java interfaces defined
// in org.chromium.content_public.browser.
//
// This is a move-only type, which makes it (almost) impossible to try to send a
// port across itself (which is illegal). This doesn't explicitly prevent you
// from sending a port's conjugate port to its conjugate, but note that the
// underlying impl will close the pipe with an error if you do that.
//
// This object is not thread safe, and is intended to be used from a single
// sequence. The sequence from which it is used does not have to be the same
// sequence that the bound receiver uses.
//
// Further note that a MessagePort is not "reusable". That is, once it has been
// bound via SetReceiver, it is no longer transmittable (can't be passed as a
// port in part of a Message). This is enforced via runtime CHECKs.
class CONTENT_EXPORT MessagePort : public mojo::MessageReceiver {
 public:
  // See below for definitions.
  struct Message;
  class MessageReceiver;

  MessagePort();
  MessagePort(const MessagePort&) = delete;
  MessagePort(MessagePort&&);
  MessagePort& operator=(const MessagePort&) = delete;
  MessagePort& operator=(MessagePort&&);
  ~MessagePort() override;

  // Factory function for creating two ends of a message channel. The two ports
  // are conjugates of each other.
  static std::pair<MessagePort, MessagePort> CreatePair();

  // Sets a message receiver for this message port. Once bound any incoming
  // messages to this port will be routed to the provided |receiver| with
  // callbacks invoked on the provided |runner|. Note that if you set a receiver
  // *after* a pipe has already transitioned to being in error, you will not
  // receive an "OnPipeError" callback; you should instead manually check
  // "is_errored" before setting the receiver. Once a receiver has been set a
  // MessagePort is no longer transferable.
  void SetReceiver(MessageReceiver* receiver,
                   scoped_refptr<base::SequencedTaskRunner> runner);

  // Clears the message receiver for this message port. Without a receiver
  // incoming messages will be queued on the port until a receiver is set.
  // Note that it is possible that there are pending message tasks already
  // posted to the previous |receiver|, thus the previous |receiver| may
  // continue to be invoked after calling this.
  void ClearReceiver();

  // Returns true if this MessagePort currently has a receiver.
  bool HasReceiver() const { return receiver_; }

  // Returns the receiver to which this MessagePort is bound. This can return
  // nullptr if it has not been bound to a receiver.
  MessageReceiver* receiver() const { return receiver_; }

  // Returns the task runner to which this MessagePort is bound. This can
  // return nullptr if the port is not bound to a receiver.
  base::SequencedTaskRunner* GetTaskRunner() const;

  // Returns true if its safe to post a message to this message port. That is,
  // a receiver has been set and the pipe is open and not in an error state.
  bool CanPostMessage() const;

  // Transmits a |message| over this port. If the port is in a state such that
  // "CanPostMessage" returns false then the message is dropped and this will
  // return false. Returns true if the message was actually accepted to be sent.
  // Note that this does not guarantee delivery, as the other end of the pipe
  // could be closed before the message is processed on the remote end.
  bool PostMessage(Message&& message);

  // Returns true if this port is bound to a valid message pipe.
  bool IsValid() const;

  // Returns true if this MessagePort has been closed.
  bool is_closed() const { return is_closed_; }

  // Returns true if this MessagePort has experienced an error.
  bool is_errored() const { return is_errored_; }

  // Returns true if this MessagePort is transferable as part of a Message. This
  // is true for a brand new MessagePort, but becomes false if SetReceiver is
  // ever called.
  bool is_transferable() const { return is_transferable_; }

  // Closes this message port. This also clears the receiver, if it is set.
  // After calling this "is_closed" will return true, "is_transferable" will
  // return false, and "is_errored" will retain the state it had before the pipe
  // was closed. This function can be called at any time, and repeatedly.
  void Close();

  // Reset this MessagePort to a completely default state. Similar to close, but
  // also resets the "is_closed", "is_errored" and "is_transferable" states. Can
  // be called at any time, and repeatedly.
  void Reset();

  // Passes out the underlying handle. This port will be reset after calling
  // this (all of "IsValid", "is_closed" and "is_errored" will return false).
  // This can only be called if "is_transferable()" returns true.
  mojo::ScopedMessagePipeHandle PassHandle();

 private:
  // Creates a message port that wraps the provided |port|. This provided |port|
  // must be valid. This is private as it should only be called by message
  // deserialization code, or the CreatePair factory.
  explicit MessagePort(mojo::ScopedMessagePipeHandle&& port);

  void Take(MessagePort&& other);
  void OnPipeError();
  void CloseIfNecessary();

  // mojo::MessageReceiver implementation:
  bool Accept(mojo::Message* mojo_message) override;

  mojo::ScopedMessagePipeHandle port_;
  std::unique_ptr<mojo::Connector> connector_;
  bool is_closed_ = true;
  bool is_errored_ = false;
  bool is_transferable_ = false;
  MessageReceiver* receiver_ = nullptr;
};

// A very simple message format. This is a subset of a TransferableMessage, as
// many of the fields in the full message type aren't appropriate for messages
// originating from the embedder.
struct CONTENT_EXPORT MessagePort::Message {
  Message();
  Message(const Message&) = delete;
  Message(Message&&);
  Message& operator=(const Message&) = delete;
  Message& operator=(Message&&);
  ~Message();

  // Creates a message with the given |data|.
  explicit Message(const base::string16& data);

  // Creates a message with the given collection of |ports| to be transferred.
  explicit Message(std::vector<MessagePort> ports);

  // Creates a message with a single |port| to be transferred.
  explicit Message(MessagePort&& port);

  // Creates a message with |data| and a collection of |ports| to be
  // transferred.
  Message(const base::string16& data, std::vector<MessagePort> ports);

  // Creates a message with |data| and a single |port| to be transferred.
  Message(const base::string16& data, MessagePort port);

  // A UTF-16 message.
  base::string16 data;

  // Other message ports that are to be transmitted as part of this message.
  std::vector<MessagePort> ports;
};

// Interface to be implemented by receivers.
class CONTENT_EXPORT MessagePort::MessageReceiver {
 public:
  MessageReceiver();
  MessageReceiver(const MessageReceiver&) = delete;
  MessageReceiver(MessageReceiver&&) = delete;
  MessageReceiver& operator=(const MessageReceiver&) = delete;
  MessageReceiver& operator=(MessageReceiver&&) = delete;
  virtual ~MessageReceiver();

  // Invoked by incoming messages. This should return true if the message was
  // successfully handled, false otherwise. If this returns false the pipe
  // will be torn down and a call to OnPipeError will be made.
  virtual bool OnMessage(Message message);

  // Invoked when the underlying pipe has experienced an error.
  virtual void OnPipeError() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_H_
