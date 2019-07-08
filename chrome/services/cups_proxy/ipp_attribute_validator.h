// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_IPP_ATTRIBUTE_VALIDATOR_H_
#define CHROME_SERVICES_CUPS_PROXY_IPP_ATTRIBUTE_VALIDATOR_H_

#include <cups/ipp.h>

#include <string>

#include "chrome/services/cups_ipp_parser/public/mojom/ipp_parser.mojom.h"

namespace cups_proxy {

// Validates an attribute |name| of type |type| in operation |ipp_oper_id|.
// |values_count| represents number of values in the attribute. The following
// constraints are enforced:
// - only operations from predefined set are accepted
// - only attributes from predefined set are accepted
// - an attribute must be part of given operation (request or response)
// - a type of the attribute must match the specification
// - a single-value attribute cannot have more than one value
// - a set-of-values attribute cannot be empty
// Returns false <=> at least one of the constraints has been violated.
bool ValidateAttribute(ipp_op_t ipp_oper_id,
                       const std::string& name,
                       cups_ipp_parser::mojom::ValueType type,
                       size_t values_count);

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_IPP_ATTRIBUTE_VALIDATOR_H_
