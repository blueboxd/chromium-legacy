// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/test/proto_printer.h"

#include <sstream>
#include <type_traits>
#include "base/json/string_escape.h"
#include "base/logging.h"

#include "components/feed/core/proto/v2/wire/content_id.pb.h"

namespace feed {
namespace {}  // namespace

template <typename T, typename S = void>
struct IsFieldSetHelper {
  // Returns false if the |v| is a zero-value.
  static bool IsSet(const T& v) { return true; }
};
template <typename T>
struct IsFieldSetHelper<T, std::enable_if_t<std::is_scalar<T>::value>> {
  static bool IsSet(const T& v) { return !!v; }
};
template <>
struct IsFieldSetHelper<std::string> {
  static bool IsSet(const std::string& v) { return !v.empty(); }
};
template <typename T>
struct IsFieldSetHelper<google::protobuf::RepeatedPtrField<T>> {
  static bool IsSet(const google::protobuf::RepeatedPtrField<T>& v) {
    return !v.empty();
  }
};
template <typename T>
bool IsFieldSet(const T& v) {
  return IsFieldSetHelper<T>::IsSet(v);
}

class TextProtoPrinter {
 public:
  template <typename T>
  static std::string ToString(const T& v) {
    TextProtoPrinter pp;
    pp << v;
    return pp.ss_.str();
  }

 private:
  // Use partial specialization to implement field printing for repeated
  // fields and messages.
  template <typename T, typename S = void>
  struct FieldPrintHelper {
    static void Run(const std::string& name, const T& v, TextProtoPrinter* pp) {
      if (!IsFieldSet(v))
        return;
      pp->Indent();
      pp->PrintRaw(name);
      pp->PrintRaw(": ");
      *pp << v;
      pp->PrintRaw("\n");
    }
  };
  template <typename T>
  struct FieldPrintHelper<google::protobuf::RepeatedPtrField<T>> {
    static void Run(const std::string& name,
                    const google::protobuf::RepeatedPtrField<T>& v,
                    TextProtoPrinter* pp) {
      for (int i = 0; i < v.size(); ++i) {
        pp->Field(name, v[i]);
      }
    }
  };
  template <typename T>
  struct FieldPrintHelper<
      T,
      std::enable_if_t<
          std::is_base_of<google::protobuf::MessageLite, T>::value>> {
    static void Run(const std::string& name, const T& v, TextProtoPrinter* pp) {
      // Print nothing if it's empty.
      if (v.ByteSizeLong() == 0)
        return;
      pp->Indent();
      pp->PrintRaw(name);
      pp->PrintRaw(" ");
      *pp << v;
    }
  };

#define PRINT_FIELD(name) Field(#name, v.name())
#define PRINT_ONEOF(name)   \
  if (v.has_##name()) {     \
    Field(#name, v.name()); \
  }

  template <typename T>
  TextProtoPrinter& operator<<(const T& v) {
    ss_ << v;
    return *this;
  }
  TextProtoPrinter& operator<<(const std::string& v) {
    ss_ << base::GetQuotedJSONString(v);
    return *this;
  }

  TextProtoPrinter& operator<<(const feedwire::ContentId& v) {
    BeginMessage();
    PRINT_FIELD(content_domain);
    PRINT_FIELD(type);
    PRINT_FIELD(id);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::Record& v) {
    BeginMessage();
    PRINT_ONEOF(stream_data);
    PRINT_ONEOF(stream_structures);
    PRINT_ONEOF(content);
    PRINT_ONEOF(local_action);
    PRINT_ONEOF(shared_state);
    PRINT_ONEOF(next_stream_state);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::StreamData& v) {
    BeginMessage();
    PRINT_FIELD(content_id);
    PRINT_FIELD(next_page_token);
    PRINT_FIELD(consistency_token);
    PRINT_FIELD(last_added_time_millis);
    PRINT_FIELD(next_action_id);
    PRINT_FIELD(shared_state_id);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::StreamStructureSet& v) {
    BeginMessage();
    PRINT_FIELD(stream_id);
    PRINT_FIELD(sequence_number);
    PRINT_FIELD(structures);
    EndMessage();
    return *this;
  }

  TextProtoPrinter& operator<<(const feedstore::StreamStructure& v) {
    BeginMessage();
    PRINT_FIELD(operation);
    PRINT_FIELD(content_id);
    PRINT_FIELD(parent_id);
    PRINT_FIELD(type);
    PRINT_FIELD(content_info);
    EndMessage();
    return *this;
  }

  TextProtoPrinter& operator<<(const feedstore::ContentInfo& v) {
    BeginMessage();
    PRINT_FIELD(score);
    PRINT_FIELD(availability_time_seconds);
    // PRINT_FIELD(representation_data);
    // PRINT_FIELD(offline_metadata);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::Content& v) {
    BeginMessage();
    PRINT_FIELD(content_id);
    PRINT_FIELD(frame);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::StreamSharedState& v) {
    BeginMessage();
    PRINT_FIELD(content_id);
    PRINT_FIELD(shared_state_data);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::StoredAction& v) {
    BeginMessage();
    PRINT_FIELD(id);
    PRINT_FIELD(upload_attempt_count);
    // PRINT_FIELD(action);
    EndMessage();
    return *this;
  }
  TextProtoPrinter& operator<<(const feedstore::StreamAndContentState& v) {
    BeginMessage();
    PRINT_FIELD(stream_data);
    PRINT_FIELD(content);
    PRINT_FIELD(shared_state);
    EndMessage();
    return *this;
  }

  template <typename T>
  void Field(const std::string& name, const T& value) {
    FieldPrintHelper<T>::Run(name, value, this);
  }
  void BeginMessage() {
    ss_ << "{\n";
    indent_ += 2;
  }
  void EndMessage() {
    indent_ -= 2;
    Indent();
    ss_ << "}\n";
  }
  void PrintRaw(const std::string& text) { ss_ << text; }

  void Indent() {
    for (int i = 0; i < indent_; ++i)
      ss_ << ' ';
  }

  int indent_ = 0;
  std::stringstream ss_;
};  // namespace feed

std::string ToTextProto(const feedwire::ContentId& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StreamData& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StreamStructureSet& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StreamStructure& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::Content& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StreamSharedState& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StreamAndContentState& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::StoredAction& v) {
  return TextProtoPrinter::ToString(v);
}
std::string ToTextProto(const feedstore::Record& v) {
  return TextProtoPrinter::ToString(v);
}

}  // namespace feed
