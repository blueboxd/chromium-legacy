#include "third_party/blink/renderer/modules/compression/inflate_transformer.h"

#include <string.h>
#include <algorithm>
#include <limits>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"

#include "v8/include/v8.h"

namespace blink {

InflateTransformer::InflateTransformer(ScriptState* script_state,
                                       Algorithm algorithm)
    : script_state_(script_state), out_buffer_(kBufferSize) {
  memset(&stream_, 0, sizeof(z_stream));
  int err;
  constexpr int kWindowBits = 15;
  constexpr int kUseGzip = 16;
  switch (algorithm) {
    case Algorithm::kDeflate:
      err = inflateInit2(&stream_, kWindowBits);
      break;
    case Algorithm::kGzip:
      err = inflateInit2(&stream_, kWindowBits + kUseGzip);
      break;
  }
  DCHECK_EQ(Z_OK, err);
}

InflateTransformer::~InflateTransformer() {
  if (!was_flush_called_) {
    inflateEnd(&stream_);
  }
}

void InflateTransformer::Transform(
    v8::Local<v8::Value> chunk,
    TransformStreamDefaultControllerInterface* controller,
    ExceptionState& exception_state) {
  // TODO(canonmukai): Change to MaybeShared<DOMUint8Array>
  // when we are ready to support SharedArrayBuffer.
  NotShared<DOMUint8Array> chunk_data = ToNotShared<NotShared<DOMUint8Array>>(
      script_state_->GetIsolate(), chunk, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  if (!chunk_data) {
    exception_state.ThrowTypeError("chunk is not of type Uint8Array.");
    return;
  }

  Inflate(chunk_data.View(), IsFinished(false), controller, exception_state);
}

void InflateTransformer::Flush(
    TransformStreamDefaultControllerInterface* controller,
    ExceptionState& exception_state) {
  DCHECK(!was_flush_called_);
  Inflate(nullptr, IsFinished(true), controller, exception_state);
  inflateEnd(&stream_);
  was_flush_called_ = true;
}

void InflateTransformer::Inflate(
    const DOMUint8Array* data,
    IsFinished finished,
    TransformStreamDefaultControllerInterface* controller,
    ExceptionState& exception_state) {
  unsigned int out_buffer_size = static_cast<unsigned int>(kBufferSize);
  if (data) {
    stream_.avail_in = data->length();
    stream_.next_in = data->DataMaybeShared();
  } else {
    stream_.avail_in = 0;
    stream_.next_in = nullptr;
  }

  do {
    stream_.avail_out = out_buffer_size;
    stream_.next_out = out_buffer_.data();
    int err = inflate(&stream_, finished ? Z_FINISH : Z_NO_FLUSH);
    if (err != Z_OK && err != Z_STREAM_END && err != Z_BUF_ERROR) {
      exception_state.ThrowTypeError("The compressed data was not valid.");
      return;
    }

    wtf_size_t bytes = out_buffer_size - stream_.avail_out;
    if (bytes) {
      controller->Enqueue(
          ToV8(DOMUint8Array::Create(out_buffer_.data(), bytes), script_state_),
          exception_state);
      if (exception_state.HadException()) {
        return;
      }
    }
  } while (stream_.avail_out == 0);
}

void InflateTransformer::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  TransformStreamTransformer::Trace(visitor);
}

}  // namespace blink
