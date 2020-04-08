#ifndef RLWE_STATUS_MACROS_H_
#define RLWE_STATUS_MACROS_H_

#include "absl/status/status.h"
#include "statusor.h"

// Helper macro that checks if the right hand side (rexpression) evaluates to a
// StatusOr with Status OK, and if so assigns the value to the value on the left
// hand side (lhs), otherwise returns the error status. Example:
//   RLWE_RLWE_ASSIGN_OR_RETURN(lhs, rexpression);
#define RLWE_ASSIGN_OR_RETURN(lhs, rexpr) \
  RLWE_ASSIGN_OR_RETURN_IMPL_(            \
      RLWE_STATUS_MACROS_IMPL_CONCAT_(_status_or_value, __LINE__), lhs, rexpr)

// Internal helper.
#define RLWE_ASSIGN_OR_RETURN_IMPL_(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                                \
  if (ABSL_PREDICT_FALSE(!statusor.ok())) {               \
    return std::move(statusor).status();                  \
  }                                                       \
  lhs = std::move(statusor).ValueOrDie()

// Internal helper for concatenating macro values.
#define RLWE_STATUS_MACROS_IMPL_CONCAT_INNER_(x, y) x##y
#define RLWE_STATUS_MACROS_IMPL_CONCAT_(x, y) \
  RLWE_STATUS_MACROS_IMPL_CONCAT_INNER_(x, y)

#define RLWE_RETURN_IF_ERROR(expr) \
  RLWE_RETURN_IF_ERROR_IMPL_(      \
      RLWE_STATUS_MACROS_IMPL_CONCAT_(_status, __LINE__), expr)

#define RLWE_RETURN_IF_ERROR_IMPL_(status, expr) \
  auto status = (expr);                          \
  if (ABSL_PREDICT_FALSE(!status.ok())) {        \
    return status;                               \
  }

#endif  // RLWE_STATUS_MACROS_H_
