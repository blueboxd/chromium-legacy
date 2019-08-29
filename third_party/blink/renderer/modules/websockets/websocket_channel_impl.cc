/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/websockets/websocket_channel_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/base_fetch_context.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/websockets/inspector_websocket_events.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_client.h"
#include "third_party/blink/renderer/modules/websockets/websocket_handle_impl.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/network/network_log.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

enum WebSocketOpCode {
  kOpCodeText = 0x1,
  kOpCodeBinary = 0x2,
};

}  // namespace

class WebSocketChannelImpl::BlobLoader final
    : public GarbageCollectedFinalized<WebSocketChannelImpl::BlobLoader>,
      public FileReaderLoaderClient {
 public:
  BlobLoader(scoped_refptr<BlobDataHandle>,
             WebSocketChannelImpl*,
             scoped_refptr<base::SingleThreadTaskRunner>);
  ~BlobLoader() override = default;

  void Cancel();

  // FileReaderLoaderClient functions.
  void DidStartLoading() override {}
  void DidReceiveData() override {}
  void DidFinishLoading() override;
  void DidFail(FileErrorCode) override;

  void Trace(blink::Visitor* visitor) { visitor->Trace(channel_); }

 private:
  Member<WebSocketChannelImpl> channel_;
  std::unique_ptr<FileReaderLoader> loader_;
};

class WebSocketChannelImpl::Message
    : public GarbageCollectedFinalized<WebSocketChannelImpl::Message> {
 public:
  Message(const std::string&, base::OnceClosure completion_callback);
  explicit Message(scoped_refptr<BlobDataHandle>);
  Message(DOMArrayBuffer*, base::OnceClosure completion_callback);
  // Close message
  Message(uint16_t code, const String& reason);

  void Trace(blink::Visitor* visitor) { visitor->Trace(array_buffer); }

  MessageType type;

  std::string text;
  scoped_refptr<BlobDataHandle> blob_data_handle;
  Member<DOMArrayBuffer> array_buffer;
  uint16_t code;
  String reason;
  base::OnceClosure completion_callback;
};

WebSocketChannelImpl::BlobLoader::BlobLoader(
    scoped_refptr<BlobDataHandle> blob_data_handle,
    WebSocketChannelImpl* channel,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : channel_(channel),
      loader_(std::make_unique<FileReaderLoader>(
          FileReaderLoader::kReadAsArrayBuffer,
          this,
          std::move(task_runner))) {
  loader_->Start(std::move(blob_data_handle));
}

void WebSocketChannelImpl::BlobLoader::Cancel() {
  loader_->Cancel();
  loader_ = nullptr;
}

void WebSocketChannelImpl::BlobLoader::DidFinishLoading() {
  channel_->DidFinishLoadingBlob(loader_->ArrayBufferResult());
  loader_ = nullptr;
}

void WebSocketChannelImpl::BlobLoader::DidFail(FileErrorCode error_code) {
  channel_->DidFailLoadingBlob(error_code);
  loader_ = nullptr;
}

struct WebSocketChannelImpl::ConnectInfo {
  ConnectInfo(const String& selected_protocol, const String& extensions)
      : selected_protocol(selected_protocol), extensions(extensions) {}

  const String selected_protocol;
  const String extensions;
};

// static
WebSocketChannelImpl* WebSocketChannelImpl::CreateForTesting(
    ExecutionContext* execution_context,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location,
    std::unique_ptr<WebSocketHandshakeThrottle> handshake_throttle) {
  auto* channel = MakeGarbageCollected<WebSocketChannelImpl>(
      execution_context, client, std::move(location),
      std::make_unique<WebSocketHandleImpl>(
          execution_context->GetTaskRunner(TaskType::kNetworking)));
  channel->handshake_throttle_ = std::move(handshake_throttle);
  return channel;
}

// static
WebSocketChannelImpl* WebSocketChannelImpl::Create(
    ExecutionContext* execution_context,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location) {
  auto* channel = MakeGarbageCollected<WebSocketChannelImpl>(
      execution_context, client, std::move(location),
      std::make_unique<WebSocketHandleImpl>(
          execution_context->GetTaskRunner(TaskType::kNetworking)));
  channel->handshake_throttle_ =
      channel->GetBaseFetchContext()->CreateWebSocketHandshakeThrottle();
  return channel;
}

WebSocketChannelImpl::WebSocketChannelImpl(
    ExecutionContext* execution_context,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location,
    std::unique_ptr<WebSocketHandle> handle)
    : handle_(std::move(handle)),
      client_(client),
      identifier_(CreateUniqueIdentifier()),
      message_chunks_(execution_context->GetTaskRunner(TaskType::kNetworking)),
      execution_context_(execution_context),
      location_at_construction_(std::move(location)),
      file_reading_task_runner_(
          execution_context->GetTaskRunner(TaskType::kFileReading)) {
  if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_))
    scope->EnsureFetcher();
}

WebSocketChannelImpl::~WebSocketChannelImpl() {
  DCHECK(!blob_loader_);
}

bool WebSocketChannelImpl::Connect(const KURL& url, const String& protocol) {
  NETWORK_DVLOG(1) << this << " Connect()";
  if (!handle_)
    return false;

  if (GetBaseFetchContext()->ShouldBlockWebSocketByMixedContentCheck(url))
    return false;

  if (auto* scheduler = execution_context_->GetScheduler()) {
    feature_handle_for_scheduler_ = scheduler->RegisterFeature(
        SchedulingPolicy::Feature::kWebSocket,
        {SchedulingPolicy::DisableAggressiveThrottling(),
         SchedulingPolicy::RecordMetricsForBackForwardCache()});
  }

  if (MixedContentChecker::IsMixedContent(
          execution_context_->GetSecurityOrigin(), url)) {
    String message =
        "Connecting to a non-secure WebSocket server from a secure origin is "
        "deprecated.";
    execution_context_->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kWarning, message));
  }

  url_ = url;
  Vector<String> protocols;
  // Avoid placing an empty token in the Vector when the protocol string is
  // empty.
  if (!protocol.IsEmpty()) {
    // Since protocol is already verified and escaped, we can simply split
    // it.
    protocol.Split(", ", true, protocols);
  }

  // If the connection needs to be filtered, asynchronously fail. Synchronous
  // failure blocks the worker thread which should be avoided. Note that
  // returning "true" just indicates that this was not a mixed content error.
  if (ShouldDisallowConnection(url)) {
    execution_context_->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&WebSocketChannelImpl::TearDownFailedConnection,
                             WrapPersistent(this)));
    return true;
  }

  mojo::Remote<mojom::blink::WebSocketConnector> connector;
  if (execution_context_->GetInterfaceProvider()) {
    execution_context_->GetInterfaceProvider()->GetInterface(
        connector.BindNewPipeAndPassReceiver(
            execution_context_->GetTaskRunner(TaskType::kWebSocket)));
  } else {
    // Create a fake request. This will lead to a closed WebSocket due to
    // a mojo connection error.
    ignore_result(connector.BindNewPipeAndPassReceiver());
  }
  handle_->Connect(std::move(connector), url, protocols,
                   GetBaseFetchContext()->GetSiteForCookies(),
                   execution_context_->UserAgent(), this);

  if (handshake_throttle_) {
    // The use of WrapWeakPersistent is safe and motivated by the fact that if
    // the WebSocket is no longer referenced, there's no point in keeping it
    // alive just to receive the throttling result.
    handshake_throttle_->ThrottleHandshake(
        url, WTF::Bind(&WebSocketChannelImpl::OnCompletion,
                       WrapWeakPersistent(this)));
  } else {
    // Treat no throttle as success.
    throttle_passed_ = true;
  }

  TRACE_EVENT_INSTANT1("devtools.timeline", "WebSocketCreate",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorWebSocketCreateEvent::Data(
                           execution_context_, identifier_, url, protocol));
  probe::DidCreateWebSocket(execution_context_, identifier_, url, protocol);
  return true;
}

WebSocketChannel::SendResult WebSocketChannelImpl::Send(
    const std::string& message,
    base::OnceClosure completion_callback) {
  NETWORK_DVLOG(1) << this << " Send(" << message << ") (std::string argument)";
  probe::DidSendWebSocketMessage(execution_context_, identifier_,
                                 WebSocketOpCode::kOpCodeText, true,
                                 message.c_str(), message.length());
  if (messages_.empty() &&
      MaybeSendSynchronously(WebSocketHandle::kMessageTypeText, message)) {
    return SendResult::SENT_SYNCHRONOUSLY;
  }

  messages_.push_back(
      MakeGarbageCollected<Message>(message, std::move(completion_callback)));

  ProcessSendQueue();

  // If we managed to flush this message synchronously after all, it would mean
  // that the callback was fired re-entrantly, which would be bad.
  DCHECK(!messages_.empty());

  return SendResult::CALLBACK_WILL_BE_CALLED;
}

void WebSocketChannelImpl::Send(
    scoped_refptr<BlobDataHandle> blob_data_handle) {
  NETWORK_DVLOG(1) << this << " Send(" << blob_data_handle->Uuid() << ", "
                   << blob_data_handle->GetType() << ", "
                   << blob_data_handle->size() << ") "
                   << "(BlobDataHandle argument)";
  // FIXME: We can't access the data here.
  // Since Binary data are not displayed in Inspector, this does not
  // affect actual behavior.
  probe::DidSendWebSocketMessage(execution_context_, identifier_,
                                 WebSocketOpCode::kOpCodeBinary, true, "", 0);
  messages_.push_back(
      MakeGarbageCollected<Message>(std::move(blob_data_handle)));
  ProcessSendQueue();
}

WebSocketChannel::SendResult WebSocketChannelImpl::Send(
    const DOMArrayBuffer& buffer,
    unsigned byte_offset,
    unsigned byte_length,
    base::OnceClosure completion_callback) {
  NETWORK_DVLOG(1) << this << " Send(" << buffer.Data() << ", " << byte_offset
                   << ", " << byte_length << ") "
                   << "(DOMArrayBuffer argument)";
  probe::DidSendWebSocketMessage(
      execution_context_, identifier_, WebSocketOpCode::kOpCodeBinary, true,
      static_cast<const char*>(buffer.Data()) + byte_offset, byte_length);
  if (messages_.empty() &&
      MaybeSendSynchronously(
          WebSocketHandle::kMessageTypeBinary,
          base::make_span(static_cast<const char*>(buffer.Data()) + byte_offset,
                          byte_length))) {
    return SendResult::SENT_SYNCHRONOUSLY;
  }

  // buffer.Slice copies its contents.
  messages_.push_back(MakeGarbageCollected<Message>(
      buffer.Slice(byte_offset, byte_offset + byte_length),
      std::move(completion_callback)));

  ProcessSendQueue();

  // If we managed to flush this message synchronously after all, it would mean
  // that the callback was fired re-entrantly, which would be bad.
  DCHECK(!messages_.empty());

  return SendResult::CALLBACK_WILL_BE_CALLED;
}

void WebSocketChannelImpl::Close(int code, const String& reason) {
  NETWORK_DVLOG(1) << this << " Close(" << code << ", " << reason << ")";
  DCHECK(handle_);
  uint16_t code_to_send = static_cast<uint16_t>(
      code == kCloseEventCodeNotSpecified ? kCloseEventCodeNoStatusRcvd : code);
  messages_.push_back(MakeGarbageCollected<Message>(code_to_send, reason));
  ProcessSendQueue();
}

void WebSocketChannelImpl::Fail(const String& reason,
                                mojom::ConsoleMessageLevel level,
                                std::unique_ptr<SourceLocation> location) {
  NETWORK_DVLOG(1) << this << " Fail(" << reason << ")";
  probe::DidReceiveWebSocketMessageError(execution_context_, identifier_,
                                         reason);
  const String message =
      "WebSocket connection to '" + url_.ElidedString() + "' failed: " + reason;

  std::unique_ptr<SourceLocation> captured_location = SourceLocation::Capture();
  if (!captured_location->IsUnknown()) {
    // If we are in JavaScript context, use the current location instead
    // of passed one - it's more precise.
    location = std::move(captured_location);
  } else if (location->IsUnknown()) {
    // No information is specified by the caller. Use the line number at the
    // connection.
    location = location_at_construction_->Clone();
  }

  execution_context_->AddConsoleMessage(
      ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript, level,
                             message, std::move(location)));
  // |reason| is only for logging and should not be provided for scripts,
  // hence close reason must be empty in tearDownFailedConnection.
  TearDownFailedConnection();
}

void WebSocketChannelImpl::Disconnect() {
  NETWORK_DVLOG(1) << this << " disconnect()";
  if (identifier_) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketDestroy", TRACE_EVENT_SCOPE_THREAD,
        "data", InspectorWebSocketEvent::Data(execution_context_, identifier_));
    probe::DidCloseWebSocket(execution_context_, identifier_);
  }
  feature_handle_for_scheduler_.reset();
  AbortAsyncOperations();
  handshake_throttle_.reset();
  handle_.reset();
  client_ = nullptr;
  identifier_ = 0;
}

void WebSocketChannelImpl::ApplyBackpressure() {
  backpressure_ = true;
}

void WebSocketChannelImpl::RemoveBackpressure() {
  backpressure_ = false;
  handle_->ConsumePendingDataFrames();
}

WebSocketChannelImpl::Message::Message(const std::string& text,
                                       base::OnceClosure completion_callback)
    : type(kMessageTypeText),
      text(text),
      completion_callback(std::move(completion_callback)) {}

WebSocketChannelImpl::Message::Message(
    scoped_refptr<BlobDataHandle> blob_data_handle)
    : type(kMessageTypeBlob), blob_data_handle(std::move(blob_data_handle)) {}

WebSocketChannelImpl::Message::Message(DOMArrayBuffer* array_buffer,
                                       base::OnceClosure completion_callback)
    : type(kMessageTypeArrayBuffer),
      array_buffer(array_buffer),
      completion_callback(std::move(completion_callback)) {}

WebSocketChannelImpl::Message::Message(uint16_t code, const String& reason)
    : type(kMessageTypeClose), code(code), reason(reason) {}

void WebSocketChannelImpl::SendInternal(
    WebSocketHandle::MessageType message_type,
    const char* data,
    wtf_size_t total_size,
    uint64_t* consumed_buffered_amount) {
  WebSocketHandle::MessageType frame_type =
      sent_size_of_top_message_ ? WebSocketHandle::kMessageTypeContinuation
                                : message_type;
  DCHECK_GE(total_size, sent_size_of_top_message_);
  // The first cast is safe since the result of min() never exceeds
  // the range of wtf_size_t. The second cast is necessary to compile
  // min() on ILP32.
  wtf_size_t size = static_cast<wtf_size_t>(
      std::min(sending_quota_,
               static_cast<uint64_t>(total_size - sent_size_of_top_message_)));
  bool final = (sent_size_of_top_message_ + size == total_size);

  SendAndAdjustQuota(final, frame_type,
                     base::make_span(data + sent_size_of_top_message_, size),
                     consumed_buffered_amount);

  sent_size_of_top_message_ += size;

  if (final) {
    base::OnceClosure completion_callback =
        std::move(messages_.front()->completion_callback);
    if (!completion_callback.is_null())
      std::move(completion_callback).Run();
    messages_.pop_front();
    sent_size_of_top_message_ = 0;
  }
}

void WebSocketChannelImpl::SendAndAdjustQuota(
    bool final,
    WebSocketHandle::MessageType frame_type,
    base::span<const char> data,
    uint64_t* consumed_buffered_amount) {
  const auto size = data.size();

  // This cast is always valid because the data size is limited by
  // sending_quota_, which is controlled by the browser process and in practice
  // is always much smaller than 4GB.
  // TODO(ricea): Change the type of sending_quota_ to wtf_size_t.
  handle_->Send(final, frame_type, data.data(), static_cast<wtf_size_t>(size));
  sending_quota_ -= size;
  *consumed_buffered_amount += size;
}

bool WebSocketChannelImpl::MaybeSendSynchronously(
    WebSocketHandle::MessageType frame_type,
    base::span<const char> data) {
  DCHECK(messages_.empty());
  if (data.size() > sending_quota_)
    return false;

  uint64_t consumed_buffered_amount = 0;
  SendAndAdjustQuota(true, frame_type, data, &consumed_buffered_amount);
  if (client_ && consumed_buffered_amount > 0)
    client_->DidConsumeBufferedAmount(consumed_buffered_amount);

  return true;
}

void WebSocketChannelImpl::ProcessSendQueue() {
  DCHECK(handle_);
  uint64_t consumed_buffered_amount = 0;
  while (!messages_.IsEmpty() && !blob_loader_) {
    Message* message = messages_.front().Get();
    CHECK(message);
    if (sending_quota_ == 0 && message->type != kMessageTypeClose)
      break;
    switch (message->type) {
      case kMessageTypeText:
        SendInternal(WebSocketHandle::kMessageTypeText, message->text.data(),
                     static_cast<wtf_size_t>(message->text.length()),
                     &consumed_buffered_amount);
        break;
      case kMessageTypeBlob:
        CHECK(!blob_loader_);
        CHECK(message);
        CHECK(message->blob_data_handle);
        blob_loader_ = MakeGarbageCollected<BlobLoader>(
            message->blob_data_handle, this, file_reading_task_runner_);
        break;
      case kMessageTypeArrayBuffer:
        CHECK(message->array_buffer);
        SendInternal(WebSocketHandle::kMessageTypeBinary,
                     static_cast<const char*>(message->array_buffer->Data()),
                     message->array_buffer->ByteLength(),
                     &consumed_buffered_amount);
        break;
      case kMessageTypeClose: {
        // No message should be sent from now on.
        DCHECK_EQ(messages_.size(), 1u);
        DCHECK_EQ(sent_size_of_top_message_, 0u);
        handshake_throttle_.reset();
        handle_->Close(message->code, message->reason);
        messages_.pop_front();
        break;
      }
    }
  }
  if (client_ && consumed_buffered_amount > 0)
    client_->DidConsumeBufferedAmount(consumed_buffered_amount);
}

void WebSocketChannelImpl::AbortAsyncOperations() {
  if (blob_loader_) {
    blob_loader_->Cancel();
    blob_loader_.Clear();
  }
}

void WebSocketChannelImpl::HandleDidClose(bool was_clean,
                                          uint16_t code,
                                          const String& reason) {
  handshake_throttle_.reset();
  handle_.reset();
  AbortAsyncOperations();
  if (!client_) {
    return;
  }
  WebSocketChannelClient* client = client_;
  client_ = nullptr;
  WebSocketChannelClient::ClosingHandshakeCompletionStatus status =
      was_clean ? WebSocketChannelClient::kClosingHandshakeComplete
                : WebSocketChannelClient::kClosingHandshakeIncomplete;
  client->DidClose(status, code, reason);
}

void WebSocketChannelImpl::DidConnect(WebSocketHandle* handle,
                                      const String& selected_protocol,
                                      const String& extensions) {
  NETWORK_DVLOG(1) << this << " DidConnect(" << handle << ", "
                   << String(selected_protocol) << ", " << String(extensions)
                   << "), throttle_passed_?" << throttle_passed_;

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());
  DCHECK(client_);

  if (!throttle_passed_) {
    connect_info_ =
        std::make_unique<ConnectInfo>(selected_protocol, extensions);
    return;
  }

  handle_->StartReceiving();

  handshake_throttle_.reset();

  client_->DidConnect(selected_protocol, extensions);
}

void WebSocketChannelImpl::DidStartOpeningHandshake(
    WebSocketHandle* handle,
    network::mojom::blink::WebSocketHandshakeRequestPtr request) {
  NETWORK_DVLOG(1) << this << " DidStartOpeningHandshake(" << handle << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "WebSocketSendHandshakeRequest",
      TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorWebSocketEvent::Data(execution_context_, identifier_));
  probe::WillSendWebSocketHandshakeRequest(execution_context_, identifier_,
                                           request.get());
  handshake_request_ = std::move(request);
}

void WebSocketChannelImpl::DidFinishOpeningHandshake(
    WebSocketHandle* handle,
    network::mojom::blink::WebSocketHandshakeResponsePtr response) {
  NETWORK_DVLOG(1) << this << " DidFinishOpeningHandshake(" << handle << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "WebSocketReceiveHandshakeResponse",
      TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorWebSocketEvent::Data(execution_context_, identifier_));
  probe::DidReceiveWebSocketHandshakeResponse(execution_context_, identifier_,
                                              handshake_request_.get(),
                                              response.get());
  handshake_request_ = nullptr;
}

void WebSocketChannelImpl::DidFail(WebSocketHandle* handle,
                                   const String& message) {
  NETWORK_DVLOG(1) << this << " DidFail(" << handle << ", " << String(message)
                   << ")";

  feature_handle_for_scheduler_.reset();

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  // This function is called when the browser is required to fail the
  // WebSocketConnection. Hence we fail this channel by calling
  // |this->failAsError| function.
  FailAsError(message);
}

void WebSocketChannelImpl::DidReceiveData(WebSocketHandle* handle,
                                          bool fin,
                                          WebSocketHandle::MessageType type,
                                          const char* data,
                                          size_t size) {
  NETWORK_DVLOG(1) << this << " DidReceiveData(" << handle << ", " << fin
                   << ", " << type << ", (" << static_cast<const void*>(data)
                   << ", " << size << "))";
  DCHECK(!backpressure_);
  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());
  DCHECK(client_);
  // Non-final frames cannot be empty.
  DCHECK(fin || size);

  switch (type) {
    case WebSocketHandle::kMessageTypeText:
      DCHECK_EQ(message_chunks_.GetSize(), 0u);
      receiving_message_type_is_text_ = true;
      break;
    case WebSocketHandle::kMessageTypeBinary:
      DCHECK_EQ(message_chunks_.GetSize(), 0u);
      receiving_message_type_is_text_ = false;
      break;
    case WebSocketHandle::kMessageTypeContinuation:
      break;
  }

  const size_t message_size_so_far = message_chunks_.GetSize();
  if (message_size_so_far > std::numeric_limits<wtf_size_t>::max()) {
    message_chunks_.Clear();
    FailAsError("Message size is too large.");
    return;
  }

  if (!fin) {
    message_chunks_.Append(base::make_span(data, size));
    return;
  }

  const wtf_size_t message_size = static_cast<wtf_size_t>(message_size_so_far);
  Vector<base::span<const char>> chunks = message_chunks_.GetView();
  if (size > 0) {
    chunks.push_back(base::make_span(data, size));
  }
  auto opcode = receiving_message_type_is_text_
                    ? WebSocketOpCode::kOpCodeText
                    : WebSocketOpCode::kOpCodeBinary;
  probe::DidReceiveWebSocketMessage(execution_context_, identifier_, opcode,
                                    false, chunks);

  if (receiving_message_type_is_text_) {
    Vector<char> flatten;
    base::span<const char> span;
    if (chunks.size() > 1) {
      flatten.ReserveCapacity(message_size);
      for (const auto& chunk : chunks) {
        flatten.Append(chunk.data(), static_cast<wtf_size_t>(chunk.size()));
      }
      span = base::make_span(flatten.data(), flatten.size());
    } else if (chunks.size() == 1) {
      span = chunks[0];
    }
    String message = span.size() > 0
                         ? String::FromUTF8(span.data(), span.size())
                         : g_empty_string;
    if (message.IsNull()) {
      FailAsError("Could not decode a text frame as UTF-8.");
    } else {
      client_->DidReceiveTextMessage(message);
    }
  } else {
    client_->DidReceiveBinaryMessage(chunks);
  }
  message_chunks_.Clear();
}

void WebSocketChannelImpl::DidClose(WebSocketHandle* handle,
                                    bool was_clean,
                                    uint16_t code,
                                    const String& reason) {
  NETWORK_DVLOG(1) << this << " DidClose(" << handle << ", " << was_clean
                   << ", " << code << ", " << String(reason) << ")";

  feature_handle_for_scheduler_.reset();

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  handle_.reset();

  if (identifier_) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketDestroy", TRACE_EVENT_SCOPE_THREAD,
        "data", InspectorWebSocketEvent::Data(execution_context_, identifier_));
    probe::DidCloseWebSocket(execution_context_, identifier_);
    identifier_ = 0;
  }

  HandleDidClose(was_clean, code, reason);
}

void WebSocketChannelImpl::AddSendFlowControlQuota(WebSocketHandle* handle,
                                                   int64_t quota) {
  NETWORK_DVLOG(1) << this << " AddSendFlowControlQuota(" << handle << ", "
                   << quota << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());
  DCHECK_GE(quota, 0);

  sending_quota_ += quota;
  ProcessSendQueue();
}

void WebSocketChannelImpl::DidStartClosingHandshake(WebSocketHandle* handle) {
  NETWORK_DVLOG(1) << this << " DidStartClosingHandshake(" << handle << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  if (client_)
    client_->DidStartClosingHandshake();
}

void WebSocketChannelImpl::OnCompletion(
    const base::Optional<WebString>& console_message) {
  DCHECK(!throttle_passed_);
  DCHECK(handshake_throttle_);
  handshake_throttle_ = nullptr;

  if (console_message) {
    FailAsError(*console_message);
    return;
  }

  throttle_passed_ = true;
  if (connect_info_) {
    handle_->StartReceiving();
    client_->DidConnect(std::move(connect_info_->selected_protocol),
                        std::move(connect_info_->extensions));
    connect_info_.reset();
  }
}

void WebSocketChannelImpl::DidFinishLoadingBlob(DOMArrayBuffer* buffer) {
  blob_loader_.Clear();
  DCHECK(handle_);
  // The loaded blob is always placed on |messages_[0]|.
  DCHECK_GT(messages_.size(), 0u);
  DCHECK_EQ(messages_.front()->type, kMessageTypeBlob);
  // We replace it with the loaded blob.
  messages_.front() =
      MakeGarbageCollected<Message>(buffer, base::OnceClosure());
  ProcessSendQueue();
}

void WebSocketChannelImpl::DidFailLoadingBlob(FileErrorCode error_code) {
  blob_loader_.Clear();
  if (error_code == FileErrorCode::kAbortErr) {
    // The error is caused by cancel().
    return;
  }
  // FIXME: Generate human-friendly reason message.
  FailAsError("Failed to load Blob: error code = " +
              String::Number(static_cast<unsigned>(error_code)));
}

void WebSocketChannelImpl::TearDownFailedConnection() {
  // |handle_| and |client_| can be null here.
  feature_handle_for_scheduler_.reset();
  handshake_throttle_.reset();

  if (client_)
    client_->DidError();

  HandleDidClose(false, kCloseEventCodeAbnormalClosure, String());
}

bool WebSocketChannelImpl::ShouldDisallowConnection(const KURL& url) {
  DCHECK(handle_);
  SubresourceFilter* subresource_filter =
      GetBaseFetchContext()->GetSubresourceFilter();
  if (!subresource_filter)
    return false;
  return !subresource_filter->AllowWebSocketConnection(url);
}

BaseFetchContext* WebSocketChannelImpl::GetBaseFetchContext() const {
  ResourceFetcher* resource_fetcher = execution_context_->Fetcher();
  return static_cast<BaseFetchContext*>(&resource_fetcher->Context());
}

ExecutionContext* WebSocketChannelImpl::GetExecutionContext() {
  return execution_context_;
}

void WebSocketChannelImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(blob_loader_);
  visitor->Trace(messages_);
  visitor->Trace(client_);
  visitor->Trace(execution_context_);
  WebSocketChannel::Trace(visitor);
}

std::ostream& operator<<(std::ostream& ostream,
                         const WebSocketChannelImpl* channel) {
  return ostream << "WebSocketChannelImpl "
                 << static_cast<const void*>(channel);
}

}  // namespace blink
