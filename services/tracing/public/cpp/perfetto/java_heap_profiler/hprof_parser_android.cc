// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_parser_android.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>

#include "base/android/java_heap_dump_generator.h"
#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_data_type_android.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_instances_android.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"

namespace tracing {

namespace {

enum class HprofInstanceTypeId : uint32_t {
  STRING = 0x01,
  CLASS = 0X02,
  HEAP_DUMP_SEGMENT = 0x1C
};

static DataType GetTypeFromIndex(uint32_t type_index) {
  return static_cast<DataType>(type_index);
}

static std::string GenerateArrayIndexString(const std::string& type_name,
                                            uint32_t index) {
  return type_name + "$" + base::NumberToString(index);
}

}  // namespace

const std::string& HprofParser::StringReference::GetString() {
  if (!cached_copy) {
    cached_copy = std::make_unique<std::string>(string_position, length);
  }
  return *cached_copy;
}

HprofParser::HprofParser(const std::string& fp) : file_path_(fp) {}

HprofParser::~HprofParser() {}

HprofParser::ParseStats::ParseStats() {}

HprofParser::StringReference::StringReference(const char* string_position,
                                              size_t length)
    : string_position(string_position), length(length) {}

HprofParser::StringReference::~StringReference() {}

HprofParser::ParseResult HprofParser::ParseStringTag(uint32_t record_length_) {
  parse_stats_.num_strings++;
  ObjectId string_id = hprof_->GetId();
  DCHECK_GT(record_length_, hprof_->object_id_size_in_bytes());
  uint32_t string_length = record_length_ - hprof_->object_id_size_in_bytes();
  strings_.emplace(string_id, std::make_unique<StringReference>(
                                  hprof_->DataPosition(), string_length));
  hprof_->Skip(string_length);

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ParseClassTag() {
  parse_stats_.num_class_objects++;
  hprof_->Skip(4);
  ObjectId object_id = hprof_->GetId();
  hprof_->Skip(4);
  ObjectId class_string_name_id = hprof_->GetId();

  auto it = strings_.find(class_string_name_id);
  if (it == strings_.end()) {
    parse_stats_.result = ParseResult::STRING_ID_NOT_FOUND;
    return parse_stats_.result;
  }
  const std::string& class_name = it->second->GetString();

  class_objects_.emplace(object_id,
                         std::make_unique<ClassObject>(object_id, class_name));

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ParseClassObjectDumpSubtag() {
  parse_stats_.num_class_object_dumps++;
  ObjectId object_id = hprof_->GetId();
  ClassObject* class_obj = class_objects_[object_id].get();

  hprof_->Skip(4);
  hprof_->SkipId();
  hprof_->SkipId();
  hprof_->SkipId();
  hprof_->SkipId();
  hprof_->SkipId();
  hprof_->SkipId();

  uint32_t instance_size = hprof_->GetFourBytes();
  uint32_t constant_pool_size = hprof_->GetTwoBytes();
  for (uint32_t i = 0; i < constant_pool_size; ++i) {
    hprof_->Skip(2);
    uint32_t type_index = hprof_->GetOneByte();
    hprof_->Skip(hprof_->SizeOfType(type_index));
  }

  uint32_t num_static_fields = hprof_->GetTwoBytes();
  uint64_t static_fields_size = 0;
  for (uint32_t i = 0; i < num_static_fields; ++i) {
    auto it = strings_.find(hprof_->GetId());
    if (it == strings_.end()) {
      parse_stats_.result = ParseResult::STRING_ID_NOT_FOUND;
      return parse_stats_.result;
    }
    const std::string& static_field_name = it->second->GetString();

    uint32_t type_index = hprof_->GetOneByte();
    DataType type = GetTypeFromIndex(type_index);
    ObjectId object_id;
    if (type == DataType::OBJECT) {
      object_id = hprof_->GetId();
    } else {
      object_id = kInvalidObjectId;
      hprof_->SkipBytesByType(type);
    }
    static_fields_size += hprof_->SizeOfType(type_index);

    class_obj->static_fields.emplace_back(static_field_name, type, object_id);
  }
  class_obj->base_instance.size = static_fields_size;

  uint32_t num_instance_fields = hprof_->GetTwoBytes();
  for (uint32_t i = 0; i < num_instance_fields; ++i) {
    auto it = strings_.find(hprof_->GetId());
    if (it == strings_.end()) {
      parse_stats_.result = ParseResult::STRING_ID_NOT_FOUND;
      return parse_stats_.result;
    }
    const std::string& instance_field_name = it->second->GetString();

    DataType type = GetTypeFromIndex(hprof_->GetOneByte());

    class_obj->instance_fields.emplace_back(instance_field_name, type,
                                            kInvalidObjectId);
  }

  class_obj->instance_size = instance_size;

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ParseClassInstanceDumpSubtag() {
  parse_stats_.num_class_instance_dumps++;
  ObjectId object_id = hprof_->GetId();
  hprof_->Skip(4);
  ObjectId class_id = hprof_->GetId();
  uint32_t instance_size = hprof_->GetFourBytes();

  uint32_t temp_data_position = hprof_->offset();
  hprof_->Skip(instance_size);

  class_instances_.emplace(
      object_id,
      std::make_unique<ClassInstance>(object_id, class_id, temp_data_position));

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ParseObjectArrayDumpSubtag() {
  parse_stats_.num_object_array_dumps++;
  ObjectId object_id = hprof_->GetId();
  hprof_->Skip(4);
  uint32_t length = hprof_->GetFourBytes();
  ObjectId class_id = hprof_->GetId();

  object_array_instances_.emplace(
      object_id, std::make_unique<ObjectArrayInstance>(
                     object_id, class_id, hprof_->offset(), length,
                     length * hprof_->object_id_size_in_bytes()));

  // Skip data inside object array, to read in later.
  hprof_->Skip(length * hprof_->object_id_size_in_bytes());

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ParsePrimitiveArrayDumpSubtag() {
  parse_stats_.num_primitive_array_dumps++;
  ObjectId object_id = hprof_->GetId();
  hprof_->Skip(4);
  uint32_t length = hprof_->GetFourBytes();

  uint32_t type_index = hprof_->GetOneByte();
  DataType type = GetTypeFromIndex(type_index);
  uint64_t size = hprof_->SizeOfType(type_index) * length;

  primitive_array_instances_.emplace(
      object_id, std::make_unique<PrimitiveArrayInstance>(
                     object_id, type, GetTypeString(type_index), size));

  // Don't read in data inside primitive array.
  hprof_->Skip(size);

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ResolveClassInstanceReferences() {
  for (auto& it : class_instances_) {
    ClassInstance* class_instance = it.second.get();

    auto class_objects_it = class_objects_.find(class_instance->class_id);
    if (class_objects_it == class_objects_.end()) {
      parse_stats_.result = ParseResult::OBJECT_ID_NOT_FOUND;
      return parse_stats_.result;
    }
    ClassObject* class_obj = class_objects_it->second.get();

    class_instance->base_instance.size = class_obj->instance_size;
    class_instance->base_instance.type_name =
        class_obj->base_instance.type_name;

    hprof_->set_position(class_instance->temp_data_position);

    for (Field f : class_obj->instance_fields) {
      if (f.type != DataType::OBJECT) {
        hprof_->SkipBytesByType(f.type);
        continue;
      }

      ObjectId id = hprof_->GetId();
      Instance* base_instance = FindInstance(id);

      // If instance is not found, just move on to the next id without adding
      // any references.
      if (!base_instance)
        continue;

      base_instance->AddReference(f.name,
                                  class_instance->base_instance.object_id);
    }
  }

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ResolveObjectArrayInstanceReferences() {
  for (auto& it : object_array_instances_) {
    ObjectArrayInstance* object_array_instance = it.second.get();
    hprof_->set_position(object_array_instance->temp_data_position);

    auto class_objects_it =
        class_objects_.find(object_array_instance->class_id);
    if (class_objects_it == class_objects_.end()) {
      parse_stats_.result = ParseResult::OBJECT_ID_NOT_FOUND;
      return parse_stats_.result;
    }
    ClassObject* class_obj = class_objects_it->second.get();

    object_array_instance->base_instance.type_name =
        class_obj->base_instance.type_name;

    for (uint32_t i = 0; i < object_array_instance->temp_data_length; i++) {
      ObjectId id = hprof_->GetId();
      Instance* base_instance = FindInstance(id);

      // If instance is not found, just move on to the next id without adding
      // any references.
      if (!base_instance)
        continue;

      base_instance->AddReference(
          GenerateArrayIndexString(
              object_array_instance->base_instance.type_name, i),
          object_array_instance->base_instance.object_id);
    }
  }
  return ParseResult::PARSE_SUCCESS;
}

void HprofParser::ModifyClassObjectTypeNames() {
  for (auto& it : class_objects_) {
    std::string new_type_name =
        "java.lang.Class:" + it.second.get()->base_instance.type_name;
    it.second.get()->base_instance.type_name = new_type_name;
  }
}

HprofParser::ParseResult HprofParser::ParseHeapDumpTag(
    uint32_t record_length_) {
  parse_stats_.num_heap_dump_segments++;
  size_t end_of_record = hprof_->offset() + record_length_;
  while (hprof_->offset() < end_of_record) {
    uint32_t subtag = hprof_->GetOneByte();
    switch (subtag) {
      // We don't store information about roots so skip them.
      case 0x01:
        hprof_->Skip(hprof_->object_id_size_in_bytes() * 2);
        break;
      case 0x02:
        hprof_->Skip(hprof_->object_id_size_in_bytes() + 8);
        break;
      case 0x03:
        hprof_->Skip(hprof_->object_id_size_in_bytes() + 8);
        break;
      case 0x04:
        hprof_->Skip(hprof_->object_id_size_in_bytes() + 4);
        break;
      case 0x05:
        hprof_->Skip(hprof_->object_id_size_in_bytes());
        break;
      case 0x06:
        hprof_->Skip(hprof_->object_id_size_in_bytes() + 4);
        break;
      case 0x07:
        hprof_->Skip(hprof_->object_id_size_in_bytes());
        break;
      case 0x08:
        hprof_->Skip(hprof_->object_id_size_in_bytes() + 8);
        break;
      case 0x89:
        hprof_->Skip(hprof_->object_id_size_in_bytes());
        break;
      case 0x8a:
        hprof_->Skip(hprof_->object_id_size_in_bytes());
        break;
      case 0x8b:
        hprof_->Skip(hprof_->object_id_size_in_bytes());
        break;
      case 0x8d:
        hprof_->Skip(hprof_->object_id_size_in_bytes());
        break;
      case 0x8e:
        hprof_->Skip(hprof_->object_id_size_in_bytes() + 8);
        break;
      case 0xfe:
        hprof_->Skip(hprof_->object_id_size_in_bytes() + 4);
        break;
      case 0xff:
        hprof_->Skip(hprof_->object_id_size_in_bytes());
        break;

      case 0x20: {  // CLASS DUMP
        ParseResult result = ParseClassObjectDumpSubtag();
        if (result != ParseResult::PARSE_SUCCESS) {
          return result;
        };
        break;
      }

      case 0x21: {  // CLASS INSTANCE DUMP
        ParseResult result = ParseClassInstanceDumpSubtag();
        if (result != ParseResult::PARSE_SUCCESS) {
          return result;
        };
        break;
      }

      case 0x22: {  // OBJECT ARRAY DUMP
        ParseResult result = ParseObjectArrayDumpSubtag();
        if (result != ParseResult::PARSE_SUCCESS) {
          return result;
        };
        break;
      }

      case 0x23: {  // PRIMITIVE ARRAY DUMP
        ParseResult result = ParsePrimitiveArrayDumpSubtag();
        if (result != ParseResult::PARSE_SUCCESS) {
          return result;
        };
        break;
      }

      default:
        NOTREACHED();
    }
  }

  return ParseResult::PARSE_SUCCESS;
}

void HprofParser::ParseFileData(const unsigned char* file_data,
                                size_t file_size) {
  hprof_ = std::make_unique<HprofBuffer>(file_data, file_size);

  uint32_t id_size;
  // Skip all leading 0s until we find the |id_size|.
  while (hprof_->GetOneByte() != 0 && hprof_->HasRemaining()) {
  }
  id_size = hprof_->GetFourBytes();
  hprof_->set_id_size(id_size);

  hprof_->Skip(4);  // hightime
  hprof_->Skip(4);  // lowtime

  while (hprof_->HasRemaining()) {
    HprofInstanceTypeId tag =
        static_cast<HprofInstanceTypeId>(hprof_->GetOneByte());
    hprof_->Skip(4);  // time
    uint32_t record_length_ = hprof_->GetFourBytes();

    switch (tag) {
      case HprofInstanceTypeId::STRING: {
        if (ParseStringTag(record_length_) != ParseResult::PARSE_SUCCESS) {
          return;
        };
        break;
      }

      case HprofInstanceTypeId::CLASS: {
        if (ParseClassTag() != ParseResult::PARSE_SUCCESS) {
          return;
        };
        break;
      }

      // TODO(zhanggeorge): Test this tag match.
      case HprofInstanceTypeId::HEAP_DUMP_SEGMENT: {
        if (ParseHeapDumpTag(record_length_) != ParseResult::PARSE_SUCCESS) {
          return;
        }
        break;
      }
      default:
        hprof_->Skip(record_length_);
    }
  }

  // Currently we have all instances defined in the hprof file within four
  // separate id->instance based off instance type (ClassObject, ClassInstance,
  // ObjectArrayInstance, PrimitiveArrayInstance). The next step is to take
  // these instances and resolve references between pairs of instances.
  // We do this specifically for ClassInstances and ObjectArrayInstances.
  // For ClassInstances, we set a reference between a given class instance
  // and any instances that are instance variables of the given class instance.
  // For ObjectArrayInstances, we set a reference between objects within the
  // object array and the actual object array.
  if (ResolveClassInstanceReferences() != ParseResult::PARSE_SUCCESS) {
    return;
  }

  ParseResult object_array_instance_result =
      ResolveObjectArrayInstanceReferences();
  if (object_array_instance_result != ParseResult::PARSE_SUCCESS) {
    return;
  }

  ModifyClassObjectTypeNames();

  parse_stats_.result = ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::Parse() {
  base::ScopedFD fd(open(file_path_.c_str(), O_RDONLY));
  if (!fd.is_valid()) {
    parse_stats_.result = HprofParser::ParseResult::FAILED_TO_OPEN_FILE;
    return parse_stats_.result;
  }

  struct stat file_stats;
  if (stat(file_path_.c_str(), &file_stats) < 0) {
    parse_stats_.result = HprofParser::ParseResult::FAILED_TO_OPEN_FILE;
    return parse_stats_.result;
  }

  void* file_data =
      mmap(0, file_stats.st_size, PROT_READ, MAP_PRIVATE, fd.get(), 0);
  if (file_data == MAP_FAILED) {
    parse_stats_.result = HprofParser::ParseResult::FAILED_TO_OPEN_FILE;
    return parse_stats_.result;
  }

  ParseFileData(reinterpret_cast<const unsigned char*>(file_data),
                file_stats.st_size);

  int res = munmap(file_data, file_stats.st_size);
  DCHECK_EQ(res, 0);

  return parse_stats_.result;
}

Instance* HprofParser::FindInstance(ObjectId id) {
  auto class_instance_sub_instance_it = class_instances_.find(id);
  if (class_instance_sub_instance_it != class_instances_.end()) {
    return &class_instance_sub_instance_it->second->base_instance;
  }

  auto object_array_sub_instance_it = object_array_instances_.find(id);
  if (object_array_sub_instance_it != object_array_instances_.end()) {
    return &object_array_sub_instance_it->second->base_instance;
  }

  auto primitive_array_sub_instance_it = primitive_array_instances_.find(id);
  if (primitive_array_sub_instance_it != primitive_array_instances_.end()) {
    return &primitive_array_sub_instance_it->second->base_instance;
  }

  auto class_object_sub_instance_it = class_objects_.find(id);
  if (class_object_sub_instance_it != class_objects_.end()) {
    return &class_object_sub_instance_it->second->base_instance;
  }

  return nullptr;
}

}  // namespace tracing
