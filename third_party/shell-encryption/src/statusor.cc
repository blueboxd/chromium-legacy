#include "statusor.h"

#include "glog/logging.h"
#include "absl/status/status.h"

namespace rlwe {
namespace internal {

static const char* kInvalidStatusCtorArgMessage =
    "Status::OK is not a valid constructor argument to StatusOr<T>";
static const char* kNullObjectCtorArgMessage =
    "NULL is not a valid constructor argument to StatusOr<T*>";

absl::Status StatusOrHelper::HandleInvalidStatusCtorArg() {
  LOG(DFATAL) << kInvalidStatusCtorArgMessage;
  return absl::InternalError(kInvalidStatusCtorArgMessage);
}

absl::Status StatusOrHelper::HandleNullObjectCtorArg() {
  LOG(DFATAL) << kNullObjectCtorArgMessage;
  return absl::InternalError(kNullObjectCtorArgMessage);
}

void StatusOrHelper::Crash(const absl::Status& status) {
  LOG(FATAL) << "Attempting to fetch value instead of handling error "
             << status;
}

}  // namespace internal
}  // namespace rlwe
