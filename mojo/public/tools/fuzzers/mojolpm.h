// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_TOOLS_FUZZERS_MOJOLPM_H_
#define MOJO_PUBLIC_TOOLS_FUZZERS_MOJOLPM_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojolpm.pb.h"

#define MOJOLPM_DBG 0
#if MOJOLPM_DBG
#define mojolpmdbg(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)
#else
#define mojolpmdbg(msg, ...)
#endif

namespace mojolpm {

typedef void* TypeId;

template <typename T>
TypeId type_id() {
  static std::remove_reference<T>* ptr = nullptr;
  return &ptr;
}

#if MOJOLPM_DBG
template <typename T>
std::string type_name() {
  return std::string(__PRETTY_FUNCTION__)
      .substr(38, strlen(__PRETTY_FUNCTION__) - 39);
}
#endif

class TestcaseBase {
 public:
  virtual ~TestcaseBase() = default;
  virtual bool IsFinished() = 0;
  virtual void NextAction() = 0;
};

class Context {
  struct Storage {
    Storage();

    template <typename T>
    explicit Storage(T&& value);

    ~Storage();

    struct StorageWrapperBase {
      virtual ~StorageWrapperBase() = default;
      virtual TypeId type() const = 0;
    };

    template <typename T>
    struct StorageWrapper : StorageWrapperBase {
      StorageWrapper() = default;
      explicit StorageWrapper(T&& value);

      TypeId type() const override;
      T& value();
      const T& value() const;

     private:
      T value_;
    };

    template <typename T>
    T& get();

    template <typename T>
    const T& get() const;

    template <typename T>
    std::unique_ptr<T> release();

   private:
    std::unique_ptr<StorageWrapperBase> wrapper_;
  };

  TestcaseBase* testcase_;
  std::map<TypeId, std::map<uint32_t, Storage>> instances_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::Message message_;

 public:
  explicit Context();
  Context(const Context&) = delete;
  ~Context();

  template <typename T>
  T* GetInstance(uint32_t id);

  template <typename T>
  std::unique_ptr<T> GetAndRemoveInstance(uint32_t id);

  template <typename T>
  void RemoveInstance(uint32_t id);

  template <typename T>
  uint32_t AddInstance(T&& instance);

  template <typename T>
  uint32_t AddInstance(uint32_t id, T&& instance);

  template <typename T>
  uint32_t NextId();

  void StartTestcase(TestcaseBase* testcase,
                     scoped_refptr<base::SequencedTaskRunner> task_runner);
  void EndTestcase();
  bool IsFinished();
  void NextAction();
  void PostNextAction();

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  mojo::Message& message() { return message_; }
};

Context* GetContext();
void SetContext(Context* context);

template <typename T>
Context::Storage::Storage(T&& value) {
  wrapper_ = std::make_unique<StorageWrapper<T>>(std::move(value));
}

template <typename T>
T& Context::Storage::Storage::get() {
  // DCHECK(wrapper_->type() == type_id<T>());
  DCHECK(static_cast<StorageWrapper<T>*>(wrapper_.get()));
  return static_cast<StorageWrapper<T>*>(wrapper_.get())->value();
}

template <typename T>
const T& Context::Storage::Storage::get() const {
  // DCHECK(wrapper_->type() == type_id<T>());
  DCHECK(static_cast<StorageWrapper<T>*>(wrapper_.get()));
  return static_cast<StorageWrapper<T>*>(wrapper_.get())->value();
}

template <typename T>
std::unique_ptr<T> Context::Storage::Storage::release() {
  // DCHECK(wrapper_->type() == type_id<T>());
  DCHECK(static_cast<StorageWrapper<T>*>(wrapper_.get()));
  return std::make_unique<T>(
      std::move(static_cast<StorageWrapper<T>*>(wrapper_.get())->value()));
}

template <typename T>
Context::Storage::StorageWrapper<T>::StorageWrapper(T&& value)
    : value_(std::move(value)) {}

template <typename T>
TypeId Context::Storage::StorageWrapper<T>::type() const {
  return type_id<T>();
}

template <typename T>
T& Context::Storage::StorageWrapper<T>::value() {
  return value_;
}

template <typename T>
const T& Context::Storage::StorageWrapper<T>::value() const {
  return value_;
}

template <typename T>
T* Context::GetInstance(uint32_t id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  mojolpmdbg("getInstance(%s, %i) = ", type_name<T>().c_str(), id);
  auto instances_iter = instances_.find(type_id<T>());
  if (instances_iter == instances_.end()) {
    mojolpmdbg("failed!\n");
    return nullptr;
  } else {
    auto& instance_map = instances_iter->second;

    // normalize id to [0, max_id]
    if (instance_map.size() > 0 && instance_map.rbegin()->first < id) {
      id = id % (instance_map.rbegin()->first + 1);
    }

    // choose the first valid entry after id
    auto instance = instance_map.lower_bound(id);
    if (instance == instance_map.end()) {
      mojolpmdbg("failed!\n");
      return nullptr;
    }

    mojolpmdbg("%i\n", instance->first);
    return &instance->second.template get<T>();
  }
}

template <typename T>
std::unique_ptr<T> Context::GetAndRemoveInstance(uint32_t id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  mojolpmdbg("getAndRemoveInstance(%s, %i) = ", type_name<T>().c_str(), id);
  auto instances_iter = instances_.find(type_id<T>());
  if (instances_iter == instances_.end()) {
    mojolpmdbg("failed\n");
    return nullptr;
  } else {
    auto& instance_map = instances_iter->second;

    // normalize id to [0, max_id]
    if (instance_map.size() > 0 && instance_map.rbegin()->first < id) {
      id = id % (instance_map.rbegin()->first + 1);
    }

    // choose the first valid entry after id
    auto instance = instance_map.lower_bound(id);
    if (instance == instance_map.end()) {
      mojolpmdbg("failed!\n");
      return nullptr;
    }

    mojolpmdbg("%i\n", instance->first);
    auto result = instance->second.template release<T>();
    instance_map.erase(instance);
    return std::move(result);
  }
}

template <typename T>
void Context::RemoveInstance(uint32_t id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  mojolpmdbg("RemoveInstance(%s, %u) = ", type_name<T>().c_str(), id);
  auto instances_iter = instances_.find(type_id<T>());
  if (instances_iter != instances_.end()) {
    auto& instance_map = instances_iter->second;

    // normalize id to [0, max_id]
    if (instance_map.size() > 0 && instance_map.rbegin()->first < id) {
      id = id % (instance_map.rbegin()->first + 1);
    }

    // choose the first valid entry after id
    auto instance = instance_map.lower_bound(id);
    if (instance == instance_map.end()) {
      mojolpmdbg("failed!\n");
      return;
    }

    instance_map.erase(instance);
  } else {
    mojolpmdbg("failed!\n");
  }
}

template <typename T>
uint32_t Context::AddInstance(T&& instance) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto instances_iter = instances_.find(type_id<T>());
  uint32_t id = 1;
  if (instances_iter == instances_.end()) {
    instances_[type_id<T>()].emplace(id, std::move(instance));
  } else {
    auto& instance_map = instances_iter->second;
    auto instance_map_iter = instance_map.begin();
    while (instance_map_iter != instance_map.end()) {
      id = instance_map_iter->first + 1;
      instance_map_iter = instance_map.find(id);
    }
    instance_map.emplace(id, std::move(instance));
  }
  mojolpmdbg("addInstance(%s, %u)\n", type_name<T>().c_str(), id);
  return id;
}

template <typename T>
uint32_t Context::AddInstance(uint32_t id, T&& instance) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto instances_iter = instances_.find(type_id<T>());
  if (instances_iter == instances_.end()) {
    instances_[type_id<T>()].emplace(id, std::move(instance));
  } else {
    auto& instance_map = instances_iter->second;
    auto instance_map_iter = instance_map.find(id);
    while (instance_map_iter != instance_map.end()) {
      id = instance_map_iter->first + 1;
      instance_map_iter = instance_map.find(id);
    }
    instance_map.emplace(id, std::move(instance));
  }
  mojolpmdbg("addInstance(%s, %u)\n", type_name<T>().c_str(), id);
  return id;
}

template <typename T>
uint32_t Context::NextId() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  uint32_t id = 1;
  auto instances_iter = instances_.find(type_id<T>());
  if (instances_iter != instances_.end()) {
    auto& instance_map = instances_iter->second;
    if (instance_map.size() > 0) {
      id = instance_map.rbegin()->first + 1;
    }
  }
  return id;
}

template <typename T>
std::unique_ptr<mojo::InterfacePtr<T>> NewInstance() {
  // mojolpmdbg("default NewInstance<>\n");
  return nullptr;
}

template <typename T>
std::unique_ptr<mojo::AssociatedInterfacePtr<T>> NewAssociatedInstance() {
  // mojolpmdbg("default NewInstanceA<>\n");
  return nullptr;
}

template <typename T>
std::unique_ptr<mojo::Remote<T>> NewRemote() {
  // mojolpmdbg("default NewInstance<>\n");
  return nullptr;
}

template <typename T>
std::unique_ptr<mojo::AssociatedRemote<T>> NewAssociatedRemote() {
  // mojolpmdbg("default NewInstanceA<>\n");
  return nullptr;
}

template <typename T>
uint32_t NextId() {
  return GetContext()->NextId<T>();
}

template <typename T>
void ResetRemote(uint32_t remote_id) {
  auto remote_ref = GetContext()->GetInstance<mojo::Remote<T>>(remote_id);
  if (remote_ref) {
    remote_ref->reset();
  }
}

template <typename T>
uint32_t AddRemote(uint32_t remote_id, mojo::Remote<T>&& remote) {
  remote_id =
      GetContext()->AddInstance<mojo::Remote<T>>(remote_id, std::move(remote));
  auto remote_ref = GetContext()->GetInstance<mojo::Remote<T>>(remote_id);
  CHECK(remote_ref);
  remote_ref->set_disconnect_handler(
      base::BindOnce(&ResetRemote<T>, remote_id));
  return remote_id;
}

template <typename T>
void ResetAssociatedRemote(uint32_t remote_id) {
  auto remote_ref =
      GetContext()->GetInstance<mojo::AssociatedRemote<T>>(remote_id);
  if (remote_ref) {
    remote_ref->reset();
  }
}

template <typename T>
uint32_t AddAssociatedRemote(uint32_t remote_id,
                             mojo::AssociatedRemote<T>&& remote) {
  remote_id = GetContext()->AddInstance<mojo::AssociatedRemote<T>>(
      remote_id, std::move(remote));
  auto remote_ref =
      GetContext()->GetInstance<mojo::AssociatedRemote<T>>(remote_id);
  CHECK(remote_ref);
  remote_ref->set_disconnect_handler(
      base::BindOnce(&ResetAssociatedRemote<T>, remote_id));
  return remote_id;
}

bool FromProto(const bool& input, bool& output);
bool ToProto(const bool& input, bool& output);
bool FromProto(const ::google::protobuf::int32& input, int8_t& output);
bool ToProto(const int8_t& input, ::google::protobuf::int32& output);
bool FromProto(const ::google::protobuf::int32& input, int16_t& output);
bool ToProto(const int16_t& input, ::google::protobuf::int32& output);
bool FromProto(const ::google::protobuf::int32& input, int32_t& output);
bool ToProto(const int32_t& input, ::google::protobuf::int32& output);
bool FromProto(const ::google::protobuf::int64& input, int64_t& output);
bool ToProto(const int64_t& input, ::google::protobuf::int64& output);
bool FromProto(const ::google::protobuf::uint32& input, uint8_t& output);
bool ToProto(const uint8_t& input, ::google::protobuf::uint32& output);
bool FromProto(const ::google::protobuf::uint32& input, uint16_t& output);
bool ToProto(const uint16_t& input, ::google::protobuf::uint32& output);
bool FromProto(const ::google::protobuf::uint32& input, uint32_t& output);
bool ToProto(const uint32_t& input, ::google::protobuf::uint32& output);
bool FromProto(const ::google::protobuf::uint64& input, uint64_t& output);
bool ToProto(const uint64_t& input, ::google::protobuf::uint64& output);
bool FromProto(const double& input, double& output);
bool ToProto(const double& input, double& output);
bool FromProto(const float& input, float& output);
bool ToProto(const float& input, float& output);
bool FromProto(const std::string& input, std::string& output);
bool ToProto(const std::string& input, std::string& output);
bool FromProto(const ::mojolpm::Handle& input, mojo::ScopedHandle& output);
bool ToProto(const mojo::ScopedHandle& input, ::mojolpm::Handle& output);
bool FromProto(const ::mojolpm::DataPipeConsumerHandle& input,
               mojo::ScopedDataPipeConsumerHandle& output);
bool ToProto(const mojo::ScopedDataPipeConsumerHandle& input,
             ::mojolpm::DataPipeConsumerHandle& output);
bool FromProto(const ::mojolpm::DataPipeProducerHandle& input,
               mojo::ScopedDataPipeProducerHandle& output);
bool ToProto(const mojo::ScopedDataPipeProducerHandle& input,
             ::mojolpm::DataPipeProducerHandle& output);
bool FromProto(const ::mojolpm::MessagePipeHandle& input,
               mojo::ScopedMessagePipeHandle& output);
bool ToProto(const mojo::ScopedMessagePipeHandle& input,
             ::mojolpm::MessagePipeHandle& output);
bool FromProto(const ::mojolpm::SharedBufferHandle& input,
               mojo::ScopedSharedBufferHandle& output);
bool ToProto(const mojo::ScopedSharedBufferHandle& input,
             ::mojolpm::SharedBufferHandle& output);
bool FromProto(const ::mojolpm::PlatformHandle& input,
               mojo::PlatformHandle& output);
bool ToProto(const mojo::PlatformHandle& input,
             ::mojolpm::PlatformHandle& output);

void HandleDataPipeRead(const ::mojolpm::DataPipeRead& input);
void HandleDataPipeWrite(const ::mojolpm::DataPipeWrite& input);
void HandleDataPipeConsumerClose(const ::mojolpm::DataPipeConsumerClose& input);
void HandleDataPipeProducerClose(const ::mojolpm::DataPipeProducerClose& input);
}  // namespace mojolpm

#endif  // MOJO_PUBLIC_TOOLS_FUZZERS_MOJOLPM_H_
