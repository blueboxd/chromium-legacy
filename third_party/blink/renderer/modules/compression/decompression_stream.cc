#include "third_party/blink/renderer/modules/compression/decompression_stream.h"
#include "third_party/blink/renderer/modules/compression/inflate_transformer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

DecompressionStream* DecompressionStream::Create(
    ScriptState* script_state,
    const AtomicString& format,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<DecompressionStream>(script_state, format,
                                                   exception_state);
}

ReadableStream* DecompressionStream::readable() const {
  return transform_->Readable();
}

WritableStream* DecompressionStream::writable() const {
  return transform_->Writable();
}

void DecompressionStream::Trace(Visitor* visitor) {
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
}

DecompressionStream::DecompressionStream(ScriptState* script_state,
                                         const AtomicString& format,
                                         ExceptionState& exception_state)
    : transform_(MakeGarbageCollected<TransformStream>()) {
  InflateTransformer::Algorithm algorithm =
      InflateTransformer::Algorithm::kDeflate;
  if (format == "gzip") {
    algorithm = InflateTransformer::Algorithm::kGzip;
  } else if (format != "deflate") {
    exception_state.ThrowTypeError("Unsupported format");
    return;
  }

  transform_->Init(
      MakeGarbageCollected<InflateTransformer>(script_state, algorithm),
      script_state, exception_state);
}

}  // namespace blink
