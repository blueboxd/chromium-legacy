// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mojo/mojo.h"

#include <string>
#include <utility>

#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/frame/document_interface_broker.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/mojo/mojo_create_data_pipe_options.h"
#include "third_party/blink/renderer/core/mojo/mojo_create_data_pipe_result.h"
#include "third_party/blink/renderer/core/mojo/mojo_create_message_pipe_result.h"
#include "third_party/blink/renderer/core/mojo/mojo_create_shared_buffer_result.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

// static
MojoCreateMessagePipeResult* Mojo::createMessagePipe() {
  MojoCreateMessagePipeResult* result_dict =
      MojoCreateMessagePipeResult::Create();
  MojoCreateMessagePipeOptions options = {0};
  options.struct_size = sizeof(::MojoCreateMessagePipeOptions);
  options.flags = MOJO_CREATE_MESSAGE_PIPE_FLAG_NONE;

  mojo::ScopedMessagePipeHandle handle0, handle1;
  MojoResult result = mojo::CreateMessagePipe(&options, &handle0, &handle1);

  result_dict->setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict->setHandle0(MakeGarbageCollected<MojoHandle>(
        mojo::ScopedHandle::From(std::move(handle0))));
    result_dict->setHandle1(MakeGarbageCollected<MojoHandle>(
        mojo::ScopedHandle::From(std::move(handle1))));
  }
  return result_dict;
}

// static
MojoCreateDataPipeResult* Mojo::createDataPipe(
    const MojoCreateDataPipeOptions* options_dict) {
  MojoCreateDataPipeResult* result_dict = MojoCreateDataPipeResult::Create();

  if (!options_dict->hasElementNumBytes() ||
      !options_dict->hasCapacityNumBytes()) {
    result_dict->setResult(MOJO_RESULT_INVALID_ARGUMENT);
    return result_dict;
  }

  ::MojoCreateDataPipeOptions options = {0};
  options.struct_size = sizeof(options);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = options_dict->elementNumBytes();
  options.capacity_num_bytes = options_dict->capacityNumBytes();

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoResult result = mojo::CreateDataPipe(&options, &producer, &consumer);
  result_dict->setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict->setProducer(MakeGarbageCollected<MojoHandle>(
        mojo::ScopedHandle::From(std::move(producer))));
    result_dict->setConsumer(MakeGarbageCollected<MojoHandle>(
        mojo::ScopedHandle::From(std::move(consumer))));
  }
  return result_dict;
}

// static
MojoCreateSharedBufferResult* Mojo::createSharedBuffer(unsigned num_bytes) {
  MojoCreateSharedBufferResult* result_dict =
      MojoCreateSharedBufferResult::Create();
  MojoCreateSharedBufferOptions* options = nullptr;
  mojo::Handle handle;
  MojoResult result =
      MojoCreateSharedBuffer(num_bytes, options, handle.mutable_value());

  result_dict->setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict->setHandle(
        MakeGarbageCollected<MojoHandle>(mojo::MakeScopedHandle(handle)));
  }
  return result_dict;
}

// static
void Mojo::bindInterface(ScriptState* script_state,
                         const String& interface_name,
                         MojoHandle* request_handle,
                         const String& scope) {
  std::string name = interface_name.Utf8();
  auto handle =
      mojo::ScopedMessagePipeHandle::From(request_handle->TakeHandle());

  if (scope == "process") {
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        name.c_str(), std::move(handle));
    return;
  }

  if (auto* interface_provider =
          ExecutionContext::From(script_state)->GetInterfaceProvider()) {
    interface_provider->GetInterfaceByName(name, std::move(handle));
  }
}

// static
MojoHandle* Mojo::getDocumentInterfaceBrokerHandle(ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);
  Document* document = static_cast<Document*>(execution_context);
  DCHECK(document);

  mojo::MessagePipe pipe;
  document->BindDocumentInterfaceBroker(std::move(pipe.handle0));
  return MakeGarbageCollected<MojoHandle>(
      mojo::ScopedHandle::From(std::move(pipe.handle1)));
}

// static
MojoHandle* Mojo::replaceDocumentInterfaceBrokerForTesting(
    ScriptState* script_state,
    MojoHandle* test_broker_handle) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);
  Document* document = static_cast<Document*>(execution_context);
  DCHECK(document);

  return MakeGarbageCollected<MojoHandle>(
      mojo::ScopedHandle::From(document->SetDocumentInterfaceBrokerForTesting(
          mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(
              test_broker_handle->TakeHandle().release().value())))));
}

}  // namespace blink
