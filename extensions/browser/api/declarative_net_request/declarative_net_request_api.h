// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_API_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_API_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace api {
namespace declarative_net_request {
struct Rule;
}  // namespace declarative_net_request
}  // namespace api

namespace declarative_net_request {
enum class DynamicRuleUpdateAction;
struct ReadJSONRulesResult;
}  // namespace declarative_net_request

// Helper base class to update the set of allowed pages.
class DeclarativeNetRequestUpdateAllowedPagesFunction
    : public UIThreadExtensionFunction {
 protected:
  enum class Action {
    ADD,     // Add allowed pages.
    REMOVE,  // Remove allowed pages.
  };
  DeclarativeNetRequestUpdateAllowedPagesFunction();
  ~DeclarativeNetRequestUpdateAllowedPagesFunction() override;

  // Updates the set of allowed pages for the extension.
  ExtensionFunction::ResponseAction UpdateAllowedPages(
      const std::vector<std::string>& patterns,
      Action action);

  // ExtensionFunction override:
  bool PreRunValidation(std::string* error) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestUpdateAllowedPagesFunction);
};

// Implements the "declarativeNetRequest.addAllowedPages" extension
// function.
class DeclarativeNetRequestAddAllowedPagesFunction
    : public DeclarativeNetRequestUpdateAllowedPagesFunction {
 public:
  DeclarativeNetRequestAddAllowedPagesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.addAllowedPages",
                             DECLARATIVENETREQUEST_ADDALLOWEDPAGES)

 protected:
  ~DeclarativeNetRequestAddAllowedPagesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestAddAllowedPagesFunction);
};

// Implements the "declarativeNetRequest.removeAllowedPages" extension
// function.
class DeclarativeNetRequestRemoveAllowedPagesFunction
    : public DeclarativeNetRequestUpdateAllowedPagesFunction {
 public:
  DeclarativeNetRequestRemoveAllowedPagesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.removeAllowedPages",
                             DECLARATIVENETREQUEST_REMOVEALLOWEDPAGES)

 protected:
  ~DeclarativeNetRequestRemoveAllowedPagesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestRemoveAllowedPagesFunction);
};

// Implements the "declarativeNetRequest.getAllowedPages" extension
// function.
class DeclarativeNetRequestGetAllowedPagesFunction
    : public UIThreadExtensionFunction {
 public:
  DeclarativeNetRequestGetAllowedPagesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.getAllowedPages",
                             DECLARATIVENETREQUEST_GETALLOWEDPAGES)

 protected:
  ~DeclarativeNetRequestGetAllowedPagesFunction() override;

  // ExtensionFunction overrides:
  bool PreRunValidation(std::string* error) override;
  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestGetAllowedPagesFunction);
};

class DeclarativeNetRequestUpdateDynamicRulesFunction
    : public UIThreadExtensionFunction {
 protected:
  DeclarativeNetRequestUpdateDynamicRulesFunction();
  ~DeclarativeNetRequestUpdateDynamicRulesFunction() override;

  ExtensionFunction::ResponseAction UpdateDynamicRules(
      std::vector<api::declarative_net_request::Rule> rules,
      declarative_net_request::DynamicRuleUpdateAction action);

 private:
  // ExtensionFunction override:
  bool PreRunValidation(std::string* error) override;

  void OnDynamicRulesUpdated(base::Optional<std::string> error);

  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestUpdateDynamicRulesFunction);
};

class DeclarativeNetRequestAddDynamicRulesFunction
    : public DeclarativeNetRequestUpdateDynamicRulesFunction {
 public:
  DeclarativeNetRequestAddDynamicRulesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.addDynamicRules",
                             DECLARATIVENETREQUEST_ADDDYNAMICRULES)

 protected:
  ~DeclarativeNetRequestAddDynamicRulesFunction() override;

  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestAddDynamicRulesFunction);
};

class DeclarativeNetRequestRemoveDynamicRulesFunction
    : public DeclarativeNetRequestUpdateDynamicRulesFunction {
 public:
  DeclarativeNetRequestRemoveDynamicRulesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.removeDynamicRules",
                             DECLARATIVENETREQUEST_REMOVEDYNAMICRULES)

 protected:
  ~DeclarativeNetRequestRemoveDynamicRulesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestRemoveDynamicRulesFunction);
};

class DeclarativeNetRequestGetDynamicRulesFunction
    : public UIThreadExtensionFunction {
 public:
  DeclarativeNetRequestGetDynamicRulesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.getDynamicRules",
                             DECLARATIVENETREQUEST_GETDYNAMICRULES)

 protected:
  ~DeclarativeNetRequestGetDynamicRulesFunction() override;

  // ExtensionFunction override:
  bool PreRunValidation(std::string* error) override;
  ExtensionFunction::ResponseAction Run() override;

 private:
  void OnDynamicRulesFetched(
      declarative_net_request::ReadJSONRulesResult read_json_result);

  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestGetDynamicRulesFunction);
};

class DeclarativeNetRequestGetMatchedRulesFunction
    : public UIThreadExtensionFunction {
 public:
  DeclarativeNetRequestGetMatchedRulesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.getMatchedRules",
                             DECLARATIVENETREQUEST_GETMATCHEDRULES)

 protected:
  ~DeclarativeNetRequestGetMatchedRulesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestGetMatchedRulesFunction);
};

class DeclarativeNetRequestSetActionCountAsBadgeTextFunction
    : public UIThreadExtensionFunction {
 public:
  DeclarativeNetRequestSetActionCountAsBadgeTextFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.setActionCountAsBadgeText",
                             DECLARATIVENETREQUEST_SETACTIONCOUNTASBADGETEXT)

 protected:
  ~DeclarativeNetRequestSetActionCountAsBadgeTextFunction() override;

  ExtensionFunction::ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DeclarativeNetRequestSetActionCountAsBadgeTextFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_API_H_
