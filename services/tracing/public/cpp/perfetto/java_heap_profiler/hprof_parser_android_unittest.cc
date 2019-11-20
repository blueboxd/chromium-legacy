// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_parser_android.h"

#include "base/android/java_heap_dump_generator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_buffer_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

TEST(HprofParserTest, BasicParse) {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    VLOG(0) << "Failed to create unique temporary directory.";
    return;
  }
  const std::string file_path_str =
      temp_dir.GetPath().Append("temp_hprof.hprof").value();

  base::android::WriteJavaHeapDumpToPath(file_path_str);
  HprofParser parser(file_path_str);
  parser.Parse();

  static const uint64_t kObjectCountThreshold = 100;
  EXPECT_GT(parser.parse_stats().num_strings, kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_class_objects, kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_heap_dump_segments, kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_class_object_dumps, kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_class_instance_dumps,
            kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_object_array_dumps, kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_primitive_array_dumps,
            kObjectCountThreshold);

  EXPECT_EQ(parser.parse_stats().result,
            HprofParser::ParseResult::PARSE_SUCCESS);
}

TEST(HprofParserTest, InvalidPathWithNoDump) {
  HprofParser parser("invalid_file");
  parser.Parse();
  EXPECT_EQ(parser.parse_stats().result,
            HprofParser::ParseResult::FAILED_TO_OPEN_FILE);
}

TEST(HprofParserTest, ParseStringTag) {
  const int length = 8;
  unsigned char file_data[length]{0,   0,   0,   1,     // string_id
                                  116, 101, 115, 116};  // "test"
  HprofParser parser("dummy_file");
  parser.hprof_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);
  parser.ParseStringTag(8);

  EXPECT_NE(parser.strings_.find(1), parser.strings_.end());
  EXPECT_EQ(parser.strings_[1]->GetString(), "test");
}

TEST(HprofParserTest, ParseClassTag) {
  const int length = 20;
  unsigned char file_data[length]{0,   0,   0,   0,  0, 0, 0, 2,  // object_id
                                  0,   0,   0,   0,  0, 0, 0, 1,  // string_id
                                  116, 101, 115, 116};            // "test"
  HprofParser parser("dummy_file");
  parser.hprof_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.strings_.emplace(
      1, std::make_unique<HprofParser::StringReference>(
             reinterpret_cast<const char*>(file_data + 16), 4));

  parser.ParseClassTag();
  auto it = parser.class_objects_.find(2);

  EXPECT_NE(it, parser.class_objects_.end());
  EXPECT_EQ(it->second->base_instance.type_name, "test");
  EXPECT_EQ(it->second->base_instance.object_id, 2u);
}

TEST(HprofParserTest, ParseClassObjectDumpSubtag) {
  const int length = 64;
  unsigned char file_data[length]{
      0,   0,   0,   3,  // object_id
      0,   0,   0,   0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,   0,   0,   0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8,  // instance_size
      0,   1,  // constant_pool_size
      0,   0,
      4,                  // type_index
      0,   0,   1,        // num_static_fields
      0,   0,   0,   1,   // string_id
      2,                  // type_index
      0,   0,   0,   2,   // object_id
      0,   1,             // num_instance_fields
      0,   0,   0,   1,   // string_id
      4,                  // type_index
      116, 101, 115, 116  // "test"
  };
  HprofParser parser("dummy_file");
  parser.hprof_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.strings_.emplace(
      1, std::make_unique<HprofParser::StringReference>(
             reinterpret_cast<const char*>(file_data + 60), 4));

  parser.class_objects_.emplace(
      3, std::make_unique<ClassObject>(3, "class_obj_dummy"));

  parser.ParseClassObjectDumpSubtag();
  auto it = parser.class_objects_.find(3);

  EXPECT_NE(it, parser.class_objects_.end());

  Field static_dummy_field = it->second->static_fields.back();
  EXPECT_EQ(static_dummy_field.name, "test");
  EXPECT_EQ(static_dummy_field.type, DataType::OBJECT);
  EXPECT_EQ(static_dummy_field.object_id, 2u);

  Field instance_dummy_field = it->second->instance_fields.back();
  EXPECT_EQ(instance_dummy_field.name, "test");
  EXPECT_EQ(instance_dummy_field.type, DataType::BOOLEAN);
  EXPECT_EQ(instance_dummy_field.object_id, kInvalidObjectId);

  EXPECT_EQ(it->second->base_instance.size, 4u);
  EXPECT_EQ(it->second->instance_size, 8u);
}

TEST(HprofParserTest, ParseClassInstanceDumpSubtag) {
  const int length = 20;
  unsigned char file_data[length]{0, 0, 0, 2,              // object_id
                                  0, 0, 0, 0, 0, 0, 0, 7,  // class_id
                                  0, 0, 0, 4,              // instance_size
                                  0, 0, 0, 0};
  HprofParser parser("dummy_file");
  parser.hprof_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.ParseClassInstanceDumpSubtag();
  auto it = parser.class_instances_.find(2);

  EXPECT_NE(it, parser.class_instances_.end());
  EXPECT_EQ(it->second->class_id, 7u);
  EXPECT_EQ(it->second->base_instance.object_id, 2u);
  EXPECT_EQ(it->second->temp_data_position, 16u);
  EXPECT_EQ(it->second->base_instance.size, 4u);
}

TEST(HprofParserTest, ParseObjectArrayDumpSubtag) {
  const int length = 24;
  unsigned char file_data[length]{0, 0, 0, 2,              // object_id
                                  0, 0, 0, 0, 0, 0, 0, 2,  // length
                                  0, 0, 0, 4,              // class_id
                                  0, 0, 0, 0, 0, 0, 0, 0};
  HprofParser parser("dummy_file");
  parser.hprof_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.ParseObjectArrayDumpSubtag();
  auto it = parser.object_array_instances_.find(2);

  EXPECT_NE(it, parser.object_array_instances_.end());
  EXPECT_EQ(it->second->class_id, 4u);
  EXPECT_EQ(it->second->base_instance.object_id, 2u);
  EXPECT_EQ(it->second->temp_data_position, 16u);
  EXPECT_EQ(it->second->temp_data_length, 2u);
  EXPECT_EQ(it->second->base_instance.size, 8u);
}

TEST(HprofParserTest, ParsePrimitiveArrayDumpSubtag) {
  const int length = 17;
  unsigned char file_data[length]{0, 0, 0, 2,              // object_id
                                  0, 0, 0, 0, 0, 0, 0, 4,  // length
                                  4,                       // type_index
                                  0, 0, 0, 0};
  HprofParser parser("dummy_file");
  parser.hprof_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.ParsePrimitiveArrayDumpSubtag();
  auto it = parser.primitive_array_instances_.find(2);

  EXPECT_NE(it, parser.primitive_array_instances_.end());
  EXPECT_EQ(it->second->base_instance.object_id, 2u);
  EXPECT_EQ(it->second->type, DataType::BOOLEAN);
  EXPECT_EQ(it->second->base_instance.type_name, "bool[]");
  EXPECT_EQ(it->second->base_instance.size, 4u);
}

}  // namespace tracing
