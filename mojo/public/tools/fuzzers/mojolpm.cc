// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/tools/fuzzers/mojolpm.h"

#include "base/no_destructor.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mojolpm {

Context::Context() : message_(0, 0, 0, 0, nullptr) {}

Context::~Context() = default;

Context::Storage::Storage() = default;

Context::Storage::~Storage() = default;

void Context::StartTestcase(
    TestcaseBase* testcase,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  testcase_ = testcase;
  task_runner_ = task_runner;
}

void Context::EndTestcase() {
  instances_.clear();
  testcase_ = nullptr;
}

bool Context::IsFinished() {
  if (testcase_) {
    return testcase_->IsFinished();
  }
  return true;
}

void Context::NextAction() {
  // fprintf(stderr, "NextAction\n");
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (testcase_) {
    testcase_->NextAction();
  }
}

void Context::PostNextAction() {
  if (task_runner_) {
    task_runner_->PostTask(FROM_HERE, base::BindOnce(&Context::NextAction,
                                                     base::Unretained(this)));
  }
}

Context* g_context = nullptr;

Context* GetContext() {
  DCHECK(g_context);
  return g_context;
}

void SetContext(Context* context) {
  g_context = context;
}

bool FromProto(const bool& input, bool& output) {
  output = input;
  return true;
}

bool ToProto(const bool& input, bool& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::int32& input, int8_t& output) {
  output = input;
  return true;
}

bool ToProto(const int8_t& input, ::google::protobuf::int32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::int32& input, int16_t& output) {
  output = input;
  return true;
}

bool ToProto(const int16_t& input, ::google::protobuf::int32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::int32& input, int32_t& output) {
  output = input;
  return true;
}

bool ToProto(const int32_t& input, ::google::protobuf::int32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::int64& input, int64_t& output) {
  output = input;
  return true;
}

bool ToProto(const int64_t& input, ::google::protobuf::int64& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::uint32& input, uint8_t& output) {
  output = input;
  return true;
}

bool ToProto(const uint8_t& input, ::google::protobuf::uint32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::uint32& input, uint16_t& output) {
  output = input;
  return true;
}

bool ToProto(const uint16_t& input, ::google::protobuf::uint32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::uint32& input, uint32_t& output) {
  output = input;
  return true;
}

bool ToProto(const uint32_t& input, ::google::protobuf::uint32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::uint64& input, uint64_t& output) {
  output = input;
  return true;
}

bool ToProto(const uint64_t& input, ::google::protobuf::uint64& output) {
  output = input;
  return true;
}

bool FromProto(const double& input, double& output) {
  output = input;
  return true;
}

bool ToProto(const double& input, double& output) {
  output = input;
  return true;
}

bool FromProto(const float& input, float& output) {
  output = input;
  return true;
}

bool ToProto(const float& input, float& output) {
  output = input;
  return true;
}

bool FromProto(const std::string& input, std::string& output) {
  output = input;
  return true;
}

bool ToProto(const std::string& input, std::string& output) {
  output = input;
  return true;
}

bool FromProto(const ::mojolpm::Handle& input, mojo::ScopedHandle& output) {
  return true;
}

bool ToProto(const mojo::ScopedHandle& input, ::mojolpm::Handle& output) {
  return true;
}

bool FromProto(const ::mojolpm::DataPipeConsumerHandle& input,
               mojo::ScopedDataPipeConsumerHandle& output) {
  bool result = false;

  if (input.instance_case() == ::mojolpm::DataPipeConsumerHandle::kOld) {
    auto old = mojolpm::GetContext()
                   ->GetAndRemoveInstance<mojo::ScopedDataPipeConsumerHandle>(
                       input.old());
    if (old) {
      output = std::move(*old.release());
    }
  } else {
    MojoCreateDataPipeOptions options;
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::ScopedDataPipeProducerHandle producer;

    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = input.new_().flags();
    options.element_num_bytes = input.new_().element_num_bytes();
    options.capacity_num_bytes = input.new_().capacity_num_bytes();

    if (MOJO_RESULT_OK ==
        mojo::CreateDataPipe(&options, &producer, &consumer)) {
      result = true;
      output = std::move(consumer);
      mojolpm::GetContext()->AddInstance(std::move(producer));
    }
  }

  return result;
}

bool ToProto(const mojo::ScopedDataPipeConsumerHandle& input,
             ::mojolpm::DataPipeConsumerHandle& output) {
  return true;
}

bool FromProto(const ::mojolpm::DataPipeProducerHandle& input,
               mojo::ScopedDataPipeProducerHandle& output) {
  bool result = false;

  if (input.instance_case() == ::mojolpm::DataPipeProducerHandle::kOld) {
    auto old = mojolpm::GetContext()
                   ->GetAndRemoveInstance<mojo::ScopedDataPipeProducerHandle>(
                       input.old());
    if (old) {
      output = std::move(*old.release());
    }
  } else {
    MojoCreateDataPipeOptions options;
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::ScopedDataPipeProducerHandle producer;

    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = input.new_().flags();
    options.element_num_bytes = input.new_().element_num_bytes();
    options.capacity_num_bytes = input.new_().capacity_num_bytes();

    if (MOJO_RESULT_OK ==
        mojo::CreateDataPipe(&options, &producer, &consumer)) {
      result = true;
      output = std::move(producer);
      mojolpm::GetContext()->AddInstance(std::move(consumer));
    }
  }

  return result;
}

bool ToProto(const mojo::ScopedDataPipeProducerHandle& input,
             ::mojolpm::DataPipeProducerHandle& output) {
  return true;
}

bool FromProto(const ::mojolpm::MessagePipeHandle& input,
               mojo::ScopedMessagePipeHandle& output) {
  return true;
}

bool ToProto(const mojo::ScopedMessagePipeHandle& input,
             ::mojolpm::MessagePipeHandle& output) {
  return true;
}

bool FromProto(const ::mojolpm::SharedBufferHandle& input,
               mojo::ScopedSharedBufferHandle& output) {
  return true;
}

bool ToProto(const mojo::ScopedSharedBufferHandle& input,
             ::mojolpm::SharedBufferHandle& output) {
  return true;
}

bool FromProto(const ::mojolpm::PlatformHandle& input,
               mojo::PlatformHandle& output) {
  return true;
}

bool ToProto(const mojo::PlatformHandle& input,
             ::mojolpm::PlatformHandle& output) {
  return true;
}

void HandleDataPipeRead(const ::mojolpm::DataPipeRead& input) {
  mojo::ScopedDataPipeConsumerHandle* consumer_ptr = nullptr;

  if (input.handle().instance_case() ==
      ::mojolpm::DataPipeConsumerHandle::kOld) {
    consumer_ptr =
        mojolpm::GetContext()->GetInstance<mojo::ScopedDataPipeConsumerHandle>(
            input.handle().old());
  } else {
    MojoCreateDataPipeOptions options;
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::ScopedDataPipeProducerHandle producer;

    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = input.handle().new_().flags();
    options.element_num_bytes = input.handle().new_().element_num_bytes();
    options.capacity_num_bytes = input.handle().new_().capacity_num_bytes();

    if (MOJO_RESULT_OK ==
        mojo::CreateDataPipe(&options, &producer, &consumer)) {
      int id = mojolpm::GetContext()->AddInstance(std::move(consumer));
      mojolpm::GetContext()->AddInstance(std::move(producer));
      consumer_ptr = mojolpm::GetContext()
                         ->GetInstance<mojo::ScopedDataPipeConsumerHandle>(id);
    }
  }

  if (consumer_ptr) {
    uint32_t size = input.size();
    std::vector<char> data(size);
    consumer_ptr->get().ReadData(data.data(), &size, 0);
  }
}

void HandleDataPipeWrite(const ::mojolpm::DataPipeWrite& input) {
  mojo::ScopedDataPipeProducerHandle* producer_ptr = nullptr;

  if (input.handle().instance_case() ==
      ::mojolpm::DataPipeProducerHandle::kOld) {
    producer_ptr =
        mojolpm::GetContext()->GetInstance<mojo::ScopedDataPipeProducerHandle>(
            input.handle().old());
  } else {
    MojoCreateDataPipeOptions options;
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::ScopedDataPipeProducerHandle producer;

    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = input.handle().new_().flags();
    options.element_num_bytes = input.handle().new_().element_num_bytes();
    options.capacity_num_bytes = input.handle().new_().capacity_num_bytes();

    if (MOJO_RESULT_OK ==
        mojo::CreateDataPipe(&options, &producer, &consumer)) {
      mojolpm::GetContext()->AddInstance(std::move(consumer));
      int id = mojolpm::GetContext()->AddInstance(std::move(producer));
      producer_ptr = mojolpm::GetContext()
                         ->GetInstance<mojo::ScopedDataPipeProducerHandle>(id);
    }
  }

  if (producer_ptr) {
    uint32_t size = static_cast<uint32_t>(input.data().size());
    producer_ptr->get().WriteData(input.data().data(), &size, 0);
  }
}

void HandleDataPipeConsumerClose(
    const ::mojolpm::DataPipeConsumerClose& input) {
  mojolpm::GetContext()->RemoveInstance<mojo::ScopedDataPipeConsumerHandle>(
      input.id());
}

void HandleDataPipeProducerClose(
    const ::mojolpm::DataPipeProducerClose& input) {
  mojolpm::GetContext()->RemoveInstance<mojo::ScopedDataPipeProducerHandle>(
      input.id());
}
}  // namespace mojolpm
