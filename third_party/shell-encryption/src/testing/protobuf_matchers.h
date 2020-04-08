#ifndef RLWE_TESTING_PROTOBUF_MATCHERS_H_
#define RLWE_TESTING_PROTOBUF_MATCHERS_H_

#include <google/protobuf/message.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>

namespace rlwe {
namespace testing {

class EqualsProtoImpl
    : public ::testing::MatcherInterface<const google::protobuf::Message&> {
 public:
  EqualsProtoImpl(const google::protobuf::Message& other) : other_(&other) {}

  bool MatchAndExplain(
      const google::protobuf::Message& message,
      ::testing::MatchResultListener* listener) const override {
    if (!google::protobuf::util::MessageDifferencer::Equals(message, *other_)) {
      *listener << "protobufs were not equal";
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "is equal to another protocol buffer";
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "is not equal to another protocol buffer";
  }

 private:
  const google::protobuf::Message* other_;  // not owned
};

::testing::Matcher<const google::protobuf::Message&> EqualsProto(
    const google::protobuf::Message& other) {
  return ::testing::Matcher<const google::protobuf::Message&>(new EqualsProtoImpl(other));
}

}  // namespace testing
}  // namespace rlwe

#endif  // RLWE_TESTING_PROTOBUF_MATCHERS_H_
