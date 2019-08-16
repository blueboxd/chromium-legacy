// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/serial/serial.h"
#include "third_party/blink/renderer/modules/serial/serial_input_signals.h"
#include "third_party/blink/renderer/modules/serial/serial_options.h"
#include "third_party/blink/renderer/modules/serial/serial_output_signals.h"
#include "third_party/blink/renderer/modules/serial/serial_port_underlying_sink.h"
#include "third_party/blink/renderer/modules/serial/serial_port_underlying_source.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

namespace {

using device::mojom::SerialReceiveError;
using device::mojom::SerialSendError;

const char kResourcesExhaustedReadBuffer[] =
    "Resources exhausted allocating read buffer.";
const char kResourcesExhaustedWriteBuffer[] =
    "Resources exhausted allocation write buffer.";
const char kPortClosed[] = "The port is closed.";
const char kOpenError[] = "Failed to open serial port.";
const char kDeviceLostError[] = "The device has been lost.";
const char kSystemError[] = "An unknown system error has occurred.";
const int kMaxBufferSize = 16 * 1024 * 1024; /* 16 MiB */

DOMException* DOMExceptionFromSendError(SerialSendError error) {
  switch (error) {
    case SerialSendError::NONE:
      NOTREACHED();
      return nullptr;
    case SerialSendError::DISCONNECTED:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kNetworkError,
                                                kDeviceLostError);
    case SerialSendError::SYSTEM_ERROR:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError,
                                                kSystemError);
  }
}

DOMException* DOMExceptionFromReceiveError(SerialReceiveError error) {
  switch (error) {
    case SerialReceiveError::NONE:
      NOTREACHED();
      return nullptr;
    case SerialReceiveError::DISCONNECTED:
    case SerialReceiveError::DEVICE_LOST:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kNetworkError,
                                                kDeviceLostError);
    case SerialReceiveError::BREAK:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kBreakError);
    case SerialReceiveError::FRAME_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kFramingError);
    case SerialReceiveError::OVERRUN:
    case SerialReceiveError::BUFFER_OVERFLOW:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kBufferOverrunError);
    case SerialReceiveError::PARITY_ERROR:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kParityError);
    case SerialReceiveError::SYSTEM_ERROR:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError,
                                                kSystemError);
  }
}

}  // namespace

SerialPort::SerialPort(Serial* parent, mojom::blink::SerialPortInfoPtr info)
    : info_(std::move(info)), parent_(parent) {}

SerialPort::~SerialPort() = default;

ScriptPromise SerialPort::open(ScriptState* script_state,
                               const SerialOptions* options) {
  if (open_resolver_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "A call to open() is already in progress."));
  }

  if (port_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "The port is already open."));
  }

  auto mojo_options = device::mojom::blink::SerialConnectionOptions::New();

  if (options->baudrate() == 0) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "Requested baud rate must be greater than zero."));
  }
  mojo_options->bitrate = options->baudrate();

  switch (options->databits()) {
    case 7:
      mojo_options->data_bits = device::mojom::blink::SerialDataBits::SEVEN;
      break;
    case 8:
      mojo_options->data_bits = device::mojom::blink::SerialDataBits::EIGHT;
      break;
    default:
      return ScriptPromise::Reject(
          script_state, V8ThrowException::CreateTypeError(
                            script_state->GetIsolate(),
                            "Requested number of data bits must be 7 or 8."));
  }

  if (options->parity() == "none") {
    mojo_options->parity_bit = device::mojom::blink::SerialParityBit::NO_PARITY;
  } else if (options->parity() == "even") {
    mojo_options->parity_bit = device::mojom::blink::SerialParityBit::EVEN;
  } else if (options->parity() == "odd") {
    mojo_options->parity_bit = device::mojom::blink::SerialParityBit::ODD;
  } else {
    NOTREACHED();
  }

  switch (options->stopbits()) {
    case 1:
      mojo_options->stop_bits = device::mojom::blink::SerialStopBits::ONE;
      break;
    case 2:
      mojo_options->stop_bits = device::mojom::blink::SerialStopBits::TWO;
      break;
    default:
      return ScriptPromise::Reject(
          script_state, V8ThrowException::CreateTypeError(
                            script_state->GetIsolate(),
                            "Requested number of stop bits must be 1 or 2."));
  }

  if (options->buffersize() == 0) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(
            script_state->GetIsolate(),
            String::Format(
                "Requested buffer size (%d bytes) must be greater than zero.",
                options->buffersize())));
  }

  if (options->buffersize() > kMaxBufferSize) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(
            script_state->GetIsolate(),
            String::Format("Requested buffer size (%d bytes) is greater than "
                           "the maximum allowed (%d bytes).",
                           options->buffersize(), kMaxBufferSize)));
  }
  buffer_size_ = options->buffersize();

  mojo_options->has_cts_flow_control = true;
  mojo_options->cts_flow_control = options->rtscts();

  // Pipe handle pair for the ReadableStream.
  mojo::ScopedDataPipeConsumerHandle readable_pipe;
  mojo::ScopedDataPipeProducerHandle readable_pipe_producer;
  if (!CreateDataPipe(&readable_pipe_producer, &readable_pipe)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kQuotaExceededError,
                          kResourcesExhaustedReadBuffer));
  }

  // Pipe handle pair for the WritableStream.
  mojo::ScopedDataPipeProducerHandle writable_pipe;
  mojo::ScopedDataPipeConsumerHandle writable_pipe_consumer;
  if (!CreateDataPipe(&writable_pipe, &writable_pipe_consumer)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kQuotaExceededError,
                          kResourcesExhaustedWriteBuffer));
  }

  device::mojom::blink::SerialPortClientPtr client_ptr;
  auto client_request = mojo::MakeRequest(&client_ptr);

  parent_->GetPort(info_->token, mojo::MakeRequest(&port_));
  port_.set_connection_error_handler(
      WTF::Bind(&SerialPort::OnConnectionError, WrapWeakPersistent(this)));

  open_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto callback = WTF::Bind(&SerialPort::OnOpen, WrapPersistent(this),
                            std::move(readable_pipe), std::move(writable_pipe),
                            std::move(client_request));

  port_->Open(std::move(mojo_options), std::move(writable_pipe_consumer),
              std::move(readable_pipe_producer), std::move(client_ptr),
              std::move(callback));
  return open_resolver_->Promise();
}

ReadableStream* SerialPort::readable(ScriptState* script_state,
                                     ExceptionState& exception_state) {
  if (readable_)
    return readable_;

  if (!port_ || open_resolver_)
    return nullptr;

  mojo::ScopedDataPipeConsumerHandle readable_pipe;
  mojo::ScopedDataPipeProducerHandle readable_pipe_producer;
  if (!CreateDataPipe(&readable_pipe_producer, &readable_pipe)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kQuotaExceededError,
                                      kResourcesExhaustedReadBuffer);
    return nullptr;
  }

  port_->ClearReadError(std::move(readable_pipe_producer));
  InitializeReadableStream(script_state, std::move(readable_pipe));
  return readable_;
}

WritableStream* SerialPort::writable(ScriptState* script_state,
                                     ExceptionState& exception_state) {
  if (writable_)
    return writable_;

  if (!port_ || open_resolver_)
    return nullptr;

  mojo::ScopedDataPipeProducerHandle writable_pipe;
  mojo::ScopedDataPipeConsumerHandle writable_pipe_consumer;
  if (!CreateDataPipe(&writable_pipe, &writable_pipe_consumer)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kQuotaExceededError,
                                      kResourcesExhaustedWriteBuffer);
    return nullptr;
  }

  port_->ClearSendError(std::move(writable_pipe_consumer));
  InitializeWritableStream(script_state, std::move(writable_pipe));
  return writable_;
}

ScriptPromise SerialPort::getSignals(ScriptState* script_state) {
  if (!port_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError, kPortClosed));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  signal_resolvers_.insert(resolver);
  port_->GetControlSignals(WTF::Bind(&SerialPort::OnGetSignals,
                                     WrapPersistent(this),
                                     WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise SerialPort::setSignals(ScriptState* script_state,
                                     const SerialOutputSignals* signals) {
  if (!port_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError, kPortClosed));
  }

  auto mojo_signals = device::mojom::blink::SerialHostControlSignals::New();
  if (signals->hasDtr()) {
    mojo_signals->has_dtr = true;
    mojo_signals->dtr = signals->dtr();
  }
  if (signals->hasRts()) {
    mojo_signals->has_rts = true;
    mojo_signals->rts = signals->rts();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  signal_resolvers_.insert(resolver);
  port_->SetControlSignals(
      std::move(mojo_signals),
      WTF::Bind(&SerialPort::OnSetSignals, WrapPersistent(this),
                WrapPersistent(resolver)));
  return resolver->Promise();
}

void SerialPort::close() {
  if (underlying_source_) {
    // The ReadableStream will report "done" when the data pipe is closed.
    underlying_source_->ExpectClose();
    underlying_source_ = nullptr;
    readable_ = nullptr;
  }
  if (underlying_sink_) {
    // TODO(crbug.com/893334): Rather than triggering an error on the
    // WritableStream this should imply a call to abort() and fail if the stream
    // is locked.
    underlying_sink_->SignalErrorOnClose(DOMExceptionFromSendError(
        device::mojom::SerialSendError::DISCONNECTED));
    underlying_sink_ = nullptr;
    writable_ = nullptr;
  }
  port_.reset();
  if (client_binding_.is_bound())
    client_binding_.Unbind();
}

void SerialPort::UnderlyingSourceClosed() {
  readable_ = nullptr;
  underlying_source_ = nullptr;
}

void SerialPort::UnderlyingSinkClosed() {
  writable_ = nullptr;
  underlying_sink_ = nullptr;
}

void SerialPort::ContextDestroyed() {
  // Release connection-related resources as quickly as possible.
  port_.reset();
}

void SerialPort::Trace(Visitor* visitor) {
  visitor->Trace(parent_);
  visitor->Trace(readable_);
  visitor->Trace(underlying_source_);
  visitor->Trace(writable_);
  visitor->Trace(underlying_sink_);
  visitor->Trace(open_resolver_);
  visitor->Trace(signal_resolvers_);
  ScriptWrappable::Trace(visitor);
}

void SerialPort::Dispose() {
  // The binding holds a raw pointer to this object which must be released when
  // it becomes garbage.
  if (client_binding_.is_bound())
    client_binding_.Unbind();
}

void SerialPort::OnReadError(device::mojom::blink::SerialReceiveError error) {
  if (underlying_source_) {
    underlying_source_->SignalErrorOnClose(DOMExceptionFromReceiveError(error));
  }
}

void SerialPort::OnSendError(device::mojom::blink::SerialSendError error) {
  if (underlying_sink_) {
    underlying_sink_->SignalErrorOnClose(DOMExceptionFromSendError(error));
  }
}

bool SerialPort::CreateDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                                mojo::ScopedDataPipeConsumerHandle* consumer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = buffer_size_;

  MojoResult result = mojo::CreateDataPipe(&options, producer, consumer);
  if (result == MOJO_RESULT_OK)
    return true;

  DCHECK_EQ(result, MOJO_RESULT_RESOURCE_EXHAUSTED);
  return false;
}

void SerialPort::OnConnectionError() {
  port_.reset();
  if (client_binding_.is_bound())
    client_binding_.Unbind();

  // Move fields since rejecting a Promise can execute script.
  ScriptPromiseResolver* open_resolver = open_resolver_;
  open_resolver_ = nullptr;
  HeapHashSet<Member<ScriptPromiseResolver>> signal_resolvers;
  signal_resolvers_.swap(signal_resolvers);
  SerialPortUnderlyingSource* underlying_source = underlying_source_;
  underlying_source_ = nullptr;
  SerialPortUnderlyingSink* underlying_sink = underlying_sink_;
  underlying_sink_ = nullptr;

  if (open_resolver) {
    open_resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, kOpenError));
  }
  for (ScriptPromiseResolver* resolver : signal_resolvers) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, kDeviceLostError));
  }
  if (underlying_source) {
    underlying_source->SignalErrorOnClose(
        DOMExceptionFromReceiveError(SerialReceiveError::DISCONNECTED));
  }
  if (underlying_sink) {
    underlying_sink->SignalErrorOnClose(
        DOMExceptionFromSendError(SerialSendError::DISCONNECTED));
  }
}

void SerialPort::OnOpen(
    mojo::ScopedDataPipeConsumerHandle readable_pipe,
    mojo::ScopedDataPipeProducerHandle writable_pipe,
    device::mojom::blink::SerialPortClientRequest client_request,
    bool success) {
  ScriptState* script_state = open_resolver_->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!success) {
    ScriptPromiseResolver* resolver = open_resolver_;
    open_resolver_ = nullptr;
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, kOpenError));
    return;
  }

  ScriptState::Scope scope(script_state);
  InitializeReadableStream(script_state, std::move(readable_pipe));
  InitializeWritableStream(script_state, std::move(writable_pipe));
  client_binding_.Bind(std::move(client_request));
  open_resolver_->Resolve();
  open_resolver_ = nullptr;
}

void SerialPort::InitializeReadableStream(
    ScriptState* script_state,
    mojo::ScopedDataPipeConsumerHandle readable_pipe) {
  DCHECK(!underlying_source_);
  DCHECK(!readable_);
  underlying_source_ = MakeGarbageCollected<SerialPortUnderlyingSource>(
      script_state, this, std::move(readable_pipe));
  readable_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source_, 0);
}

void SerialPort::InitializeWritableStream(
    ScriptState* script_state,
    mojo::ScopedDataPipeProducerHandle writable_pipe) {
  DCHECK(!underlying_sink_);
  DCHECK(!writable_);
  underlying_sink_ = MakeGarbageCollected<SerialPortUnderlyingSink>(
      this, std::move(writable_pipe));
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink_, 0);
}

void SerialPort::OnGetSignals(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SerialPortControlSignalsPtr mojo_signals) {
  DCHECK(signal_resolvers_.Contains(resolver));
  signal_resolvers_.erase(resolver);

  if (!mojo_signals) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Failed to get control signals."));
    return;
  }

  auto* signals = MakeGarbageCollected<SerialInputSignals>();
  signals->setDcd(mojo_signals->dcd);
  signals->setCts(mojo_signals->cts);
  signals->setRi(mojo_signals->ri);
  signals->setDsr(mojo_signals->dsr);
  resolver->Resolve(signals);
}

void SerialPort::OnSetSignals(ScriptPromiseResolver* resolver, bool success) {
  DCHECK(signal_resolvers_.Contains(resolver));
  signal_resolvers_.erase(resolver);

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Failed to set control signals."));
    return;
  }

  resolver->Resolve();
}

}  // namespace blink
