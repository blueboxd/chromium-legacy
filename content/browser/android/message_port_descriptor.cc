// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the native implementation of MessagePortDescriptor.java,
// which wraps blink::MessagePortDescriptor from
// /third_party/blink/public/common/messaging/message_port_descriptor.h.

#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/stl_util.h"
#include "base/unguessable_token.h"
#include "content/public/android/content_jni_headers/AppWebMessagePortDescriptor_jni.h"
#include "mojo/public/cpp/system/message_pipe.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace blink {

// Helper that provides access to protected member functions of
// blink::MessagePortDescriptor.
class MessagePortJavaAccess {
 public:
  static MessagePortDescriptor::InstrumentationDelegate* GetDelegate() {
    return MessagePortDescriptor::GetInstrumentationDelegate();
  }

  static mojo::ScopedMessagePipeHandle TakeHandleToEntangleWithEmbedder(
      blink::MessagePortDescriptor* message_port_descriptor) {
    return message_port_descriptor->TakeHandleToEntangleWithEmbedder();
  }

  static void GiveDisentangledHandle(
      blink::MessagePortDescriptor* message_port_descriptor,
      mojo::ScopedMessagePipeHandle handle) {
    message_port_descriptor->GiveDisentangledHandle(std::move(handle));
  }

  static void Init(blink::MessagePortDescriptor* message_port_descriptor,
                   mojo::ScopedMessagePipeHandle handle,
                   const base::UnguessableToken& id,
                   uint64_t sequence_number) {
    message_port_descriptor->Init(std::move(handle), id, sequence_number);
  }

  static mojo::ScopedMessagePipeHandle TakeHandle(
      blink::MessagePortDescriptor* message_port_descriptor) {
    return message_port_descriptor->TakeHandle();
  }

  static base::UnguessableToken TakeId(
      blink::MessagePortDescriptor* message_port_descriptor) {
    return message_port_descriptor->TakeId();
  }

  static uint64_t TakeSequenceNumber(
      blink::MessagePortDescriptor* message_port_descriptor) {
    return message_port_descriptor->TakeSequenceNumber();
  }
};

}  // namespace blink

namespace {

mojo::ScopedMessagePipeHandle WrapNativeHandle(jint native_handle) {
  MojoHandle raw_handle = static_cast<MojoHandle>(native_handle);
  DCHECK_NE(MOJO_HANDLE_INVALID, raw_handle);
  return mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(raw_handle));
}

}  // namespace

JNI_EXPORT ScopedJavaLocalRef<jlongArray>
JNI_AppWebMessagePortDescriptor_CreatePair(JNIEnv* env) {
  // Ownership is passed to the Java code. These are cleaned up when
  // CloseAndDestroy is called.
  blink::MessagePortDescriptor* port0 = new blink::MessagePortDescriptor();
  blink::MessagePortDescriptor* port1 = new blink::MessagePortDescriptor();

  blink::MessagePortDescriptorPair pipe;
  *port0 = pipe.TakePort0();
  *port1 = pipe.TakePort1();

  uint64_t pointers[2] = {reinterpret_cast<uint64_t>(port0),
                          reinterpret_cast<uint64_t>(port1)};
  return base::android::ToJavaLongArray(
      env, reinterpret_cast<const int64_t*>(pointers), base::size(pointers));
}

JNI_EXPORT jlong JNI_AppWebMessagePortDescriptor_Create(JNIEnv* env,
                                                        jint native_handle,
                                                        jlong id_low,
                                                        jlong id_high,
                                                        jlong sequence_number) {
  base::UnguessableToken id = base::UnguessableToken::Deserialize(
      static_cast<uint64_t>(id_high), static_cast<uint64_t>(id_low));

  // Ownership is passed to the Java code. This is cleaned up when
  // CloseAndDestroy is called.
  blink::MessagePortDescriptor* port = new blink::MessagePortDescriptor();
  blink::MessagePortJavaAccess::Init(port, WrapNativeHandle(native_handle), id,
                                     sequence_number);

  return reinterpret_cast<jlong>(port);
}

JNI_EXPORT jint JNI_AppWebMessagePortDescriptor_TakeHandleToEntangle(
    JNIEnv* env,
    jlong native_message_port_decriptor) {
  blink::MessagePortDescriptor* message_port_descriptor =
      reinterpret_cast<blink::MessagePortDescriptor*>(
          native_message_port_decriptor);
  DCHECK(message_port_descriptor->IsValid());
  DCHECK(!message_port_descriptor->IsEntangled());

  // Ownership of the underlying native handle is passed to Java. It is returned
  // before tear-down via GiveDisentangledHandle.
  mojo::ScopedMessagePipeHandle handle =
      blink::MessagePortJavaAccess::TakeHandleToEntangleWithEmbedder(
          message_port_descriptor);

  return static_cast<jint>(handle.release().value());
}

JNI_EXPORT void JNI_AppWebMessagePortDescriptor_GiveDisentangledHandle(
    JNIEnv* env,
    jlong native_message_port_decriptor,
    jint native_handle) {
  blink::MessagePortDescriptor* message_port_descriptor =
      reinterpret_cast<blink::MessagePortDescriptor*>(
          native_message_port_decriptor);
  DCHECK(message_port_descriptor->IsValid());
  DCHECK(message_port_descriptor->IsEntangled());

  blink::MessagePortJavaAccess::GiveDisentangledHandle(
      message_port_descriptor, WrapNativeHandle(native_handle));
}

JNI_EXPORT ScopedJavaLocalRef<jlongArray>
JNI_AppWebMessagePortDescriptor_PassSerialized(
    JNIEnv* env,
    jlong native_message_port_decriptor) {
  blink::MessagePortDescriptor* message_port_descriptor =
      reinterpret_cast<blink::MessagePortDescriptor*>(
          native_message_port_decriptor);
  DCHECK(message_port_descriptor->IsValid());
  DCHECK(!message_port_descriptor->IsEntangled());

  // Tear down and free the native object.
  mojo::ScopedMessagePipeHandle handle =
      blink::MessagePortJavaAccess::TakeHandle(message_port_descriptor);
  base::UnguessableToken id =
      blink::MessagePortJavaAccess::TakeId(message_port_descriptor);
  uint64_t sequence_number =
      blink::MessagePortJavaAccess::TakeSequenceNumber(message_port_descriptor);
  delete message_port_descriptor;

  // Serialize its contents and pass to the Java implementation.
  uint64_t serialized[4] = {handle.release().value(),
                            id.GetLowForSerialization(),
                            id.GetHighForSerialization(), sequence_number};
  return base::android::ToJavaLongArray(
      env, reinterpret_cast<const int64_t*>(serialized),
      base::size(serialized));
}

JNI_EXPORT void JNI_AppWebMessagePortDescriptor_CloseAndDestroy(
    JNIEnv* env,
    jlong native_message_port_decriptor) {
  blink::MessagePortDescriptor* message_port_descriptor =
      reinterpret_cast<blink::MessagePortDescriptor*>(
          native_message_port_decriptor);
  DCHECK(message_port_descriptor->IsValid());
  DCHECK(!message_port_descriptor->IsEntangled());
  message_port_descriptor->Reset();
  delete message_port_descriptor;
}
