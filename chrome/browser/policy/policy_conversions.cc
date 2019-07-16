// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_conversions.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/user_manager/user_manager.h"
#endif

using base::Value;

namespace em = enterprise_management;

namespace policy {

// Fills |policy_dump| with device specific information if this device is
// enterprise managed.
void FillIdentityFields(Value* policy_dump);

namespace {

// Maps known policy names to their schema. If a policy is not present, it is
// not known (either through policy_templates.json or through an extenion's
// managed storage schema).
using PolicyToSchemaMap = base::flat_map<std::string, Schema>;

// Utility function that returns a JSON serialization of the given |dict|.
std::string DictionaryToJSONString(const Value& dict, bool is_pretty_print) {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      dict, (is_pretty_print ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0),
      &json_string);
  return json_string;
}

// Returns a copy of |value|. If necessary (which is specified by
// |convert_values|), converts some values to a representation that
// i18n_template.js will display.
Value CopyAndMaybeConvert(const Value& value,
                          bool convert_values,
                          const base::Optional<Schema>& schema,
                          bool is_pretty_print) {
  Value value_copy = value.Clone();
  if (schema.has_value())
    schema->MaskSensitiveValues(&value_copy);
  if (!convert_values)
    return value_copy;
  if (value_copy.is_dict())
    return Value(DictionaryToJSONString(value_copy, is_pretty_print));

  if (!value_copy.is_list()) {
    return value_copy;
  }

  Value result(Value::Type::LIST);
  for (const auto& element : value_copy.GetList()) {
    if (element.is_dict()) {
      result.GetList().emplace_back(
          Value(DictionaryToJSONString(element, is_pretty_print)));
    } else {
      result.GetList().push_back(element.Clone());
    }
  }
  return result;
}

PolicyService* GetPolicyService(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return profile->GetProfilePolicyConnector()->policy_service();
}

// Returns the Schema for |policy_name| if that policy is known. If the policy
// is unknown, returns |base::nullopt|.
base::Optional<Schema> GetKnownPolicySchema(
    const base::Optional<PolicyToSchemaMap>& known_policy_schemas,
    const std::string& policy_name) {
  if (!known_policy_schemas.has_value())
    return base::nullopt;
  auto known_policy_iterator = known_policy_schemas->find(policy_name);
  if (known_policy_iterator == known_policy_schemas->end())
    return base::nullopt;
  return known_policy_iterator->second;
}

// Create a description of the policy |policy_name| using |policy| and the
// optional errors in |errors| to determine the status of each policy. If
// |convert_values| is true, converts the values to show them in javascript.
// |known_policy_schemas| contains |Schema|s for known policies in the same
// policy namespace of |map|. A policy without an entry in
// |known_policy_schemas| is an unknown policy.
// When |convert_types| is true, policy types are converted into string. For
// example, POLICY_SCOPE_USER is converted to 'user'. Otherwise, policy types
// are returned as integers.
Value GetPolicyValue(
    const std::string& policy_name,
    const PolicyMap::Entry& policy,
    PolicyErrorMap* errors,
    bool convert_values,
    const base::Optional<PolicyToSchemaMap>& known_policy_schemas,
    bool is_pretty_print,
    bool convert_types) {
  base::Optional<Schema> known_policy_schema =
      GetKnownPolicySchema(known_policy_schemas, policy_name);
  Value value(Value::Type::DICTIONARY);
  value.SetKey("value",
               CopyAndMaybeConvert(*policy.value, convert_values,
                                   known_policy_schema, is_pretty_print));
  if (convert_types) {
    value.SetKey(
        "scope",
        Value((policy.scope == POLICY_SCOPE_USER) ? "user" : "machine"));
    value.SetKey("level", Value(Value((policy.level == POLICY_LEVEL_RECOMMENDED)
                                          ? "recommended"
                                          : "mandatory")));
    value.SetKey("source", Value(kPolicySources[policy.source].name));
  } else {
    value.SetKey("scope", Value(policy.scope));
    value.SetKey("level", Value(policy.level));
    value.SetKey("source", Value(policy.source));
  }
  base::string16 error;
  if (!known_policy_schema.has_value()) {
    // We don't know what this policy is. This is an important error to
    // show.
    error = l10n_util::GetStringUTF16(IDS_POLICY_UNKNOWN);
  } else {
    // The PolicyMap contains errors about retrieving the policy, while the
    // PolicyErrorMap contains validation errors. Concat the errors.
    auto policy_map_errors = policy.GetLocalizedErrors(
        base::BindRepeating(&l10n_util::GetStringUTF16));
    auto error_map_errors = errors->GetErrors(policy_name);
    if (policy_map_errors.empty())
      error = error_map_errors;
    else if (error_map_errors.empty())
      error = policy_map_errors;
    else
      error =
          base::JoinString({policy_map_errors, errors->GetErrors(policy_name)},
                           base::ASCIIToUTF16("\n"));
  }
  if (!error.empty())
    value.SetKey("error", Value(error));

  base::string16 warning = policy.GetLocalizedWarnings(
      base::BindRepeating(&l10n_util::GetStringUTF16));
  if (!warning.empty())
    value.SetKey("warning", Value(warning));

  if (policy.IsBlockedOrIgnored())
    value.SetBoolKey("ignored", true);

  if (!policy.conflicts.empty()) {
    Value conflict_values(Value::Type::LIST);
    for (const auto& conflict : policy.conflicts) {
      base::Value conflicted_policy_value =
          GetPolicyValue(policy_name, conflict, errors, convert_values,
                         known_policy_schemas, is_pretty_print, convert_types);
      conflict_values.GetList().push_back(std::move(conflicted_policy_value));
    }

    value.SetKey("conflicts", std::move(conflict_values));
  }

  return value;
}

// Inserts a description of each policy in |map| into |values|, using the
// optional errors in |errors| to determine the status of each policy. If
// |convert_values| is true, converts the values to show them in javascript.
// |known_policy_schemas| contains |Schema|s for known policies in the same
// policy namespace of |map|. A policy in |map| but without an entry
// |known_policy_schemas| is an unknown policy.
void GetPolicyValues(
    const PolicyMap& map,
    PolicyErrorMap* errors,
    bool with_user_policies,
    bool convert_values,
    const base::Optional<PolicyToSchemaMap>& known_policy_schemas,
    Value* values,
    bool is_pretty_print,
    bool convert_types) {
  DCHECK(values);
  for (const auto& entry : map) {
    const std::string& policy_name = entry.first;
    const PolicyMap::Entry& policy = entry.second;
    if (policy.scope == POLICY_SCOPE_USER && !with_user_policies)
      continue;
    base::Value value =
        GetPolicyValue(policy_name, policy, errors, convert_values,
                       known_policy_schemas, is_pretty_print, convert_types);
    values->SetKey(policy_name, std::move(value));
  }
}

base::Optional<PolicyToSchemaMap> GetKnownPolicies(
    const scoped_refptr<SchemaMap> schema_map,
    const PolicyNamespace& policy_namespace,
    bool is_pretty_print) {
  const Schema* schema = schema_map->GetSchema(policy_namespace);
  // There is no policy name verification without valid schema.
  if (!schema || !schema->valid())
    return base::nullopt;

  // Build a vector first and construct the PolicyToSchemaMap (which is a
  // |flat_map|) from that. The reason is that insertion into a |flat_map| is
  // O(n), which would make the loop O(n^2), but constructing from a
  // pre-populated vector is less expensive.
  std::vector<std::pair<std::string, Schema>> policy_to_schema_entries;
  for (auto it = schema->GetPropertiesIterator(); !it.IsAtEnd(); it.Advance()) {
    policy_to_schema_entries.push_back(std::make_pair(it.key(), it.schema()));
  }
  return PolicyToSchemaMap(std::move(policy_to_schema_entries));
}

void GetChromePolicyValues(content::BrowserContext* context,
                           bool keep_user_policies,
                           bool convert_values,
                           Value* values,
                           bool is_pretty_print,
                           bool convert_types) {
  PolicyService* policy_service = GetPolicyService(context);
  PolicyMap map;

  Profile* profile = Profile::FromBrowserContext(context);
  auto* schema_registry_service = profile->GetPolicySchemaRegistryService();
  if (!schema_registry_service || !schema_registry_service->registry()) {
    LOG(ERROR) << "Can not dump extension policies, no schema registry service";
    return;
  }

  const scoped_refptr<SchemaMap> schema_map =
      schema_registry_service->registry()->schema_map();

  PolicyNamespace policy_namespace =
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());

  // Make a copy that can be modified, since some policy values are modified
  // before being displayed.
  map.CopyFrom(policy_service->GetPolicies(policy_namespace));

  // Get a list of all the errors in the policy values.
  const ConfigurationPolicyHandlerList* handler_list =
      g_browser_process->browser_policy_connector()->GetHandlerList();
  PolicyErrorMap errors;
  handler_list->ApplyPolicySettings(map, NULL, &errors);

  // Convert dictionary values to strings for display.
  handler_list->PrepareForDisplaying(&map);

  GetPolicyValues(
      map, &errors, keep_user_policies, convert_values,
      GetKnownPolicies(schema_map, policy_namespace, is_pretty_print), values,
      is_pretty_print, convert_types);
}

#if defined(OS_CHROMEOS)
void GetDeviceLocalAccountPolicies(bool convert_values,
                                   Value* values,
                                   bool with_device_data,
                                   bool is_pretty_print,
                                   bool convert_types) {
  // DeviceLocalAccount policies are only available for affiliated users and for
  // system logs.
  if (!with_device_data &&
      (!user_manager::UserManager::IsInitialized() ||
       !user_manager::UserManager::Get()->GetPrimaryUser() ||
       !user_manager::UserManager::Get()->GetPrimaryUser()->IsAffiliated())) {
    return;
  }

  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DCHECK(connector);  // always not-null

  auto* device_local_account_policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  DCHECK(device_local_account_policy_service);  // always non null for
                                                // affiliated users
  std::vector<DeviceLocalAccount> device_local_accounts =
      GetDeviceLocalAccounts(chromeos::CrosSettings::Get());
  for (const auto& account : device_local_accounts) {
    std::string user_id = account.user_id;
    Value current_account_policies(Value::Type::DICTIONARY);

    auto* device_local_account_policy_broker =
        device_local_account_policy_service->GetBrokerForUser(user_id);
    if (!device_local_account_policy_broker) {
      LOG(ERROR)
          << "Can not get policy broker for device local account with user id: "
          << user_id;
      continue;
    }

    auto* cloud_policy_core = device_local_account_policy_broker->core();
    DCHECK(cloud_policy_core);
    auto* cloud_policy_store = cloud_policy_core->store();
    DCHECK(cloud_policy_store);

    const scoped_refptr<SchemaMap> schema_map =
        device_local_account_policy_broker->schema_registry()->schema_map();

    PolicyNamespace policy_namespace =
        PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());

    // Make a copy that can be modified, since some policy values are modified
    // before being displayed.
    PolicyMap map;
    map.CopyFrom(cloud_policy_store->policy_map());

    // Get a list of all the errors in the policy values.
    const ConfigurationPolicyHandlerList* handler_list =
        connector->GetHandlerList();
    PolicyErrorMap errors;
    handler_list->ApplyPolicySettings(map, NULL, &errors);

    // Convert dictionary values to strings for display.
    handler_list->PrepareForDisplaying(&map);

    GetPolicyValues(
        map, &errors, true, convert_values,
        GetKnownPolicies(schema_map, policy_namespace, is_pretty_print),
        &current_account_policies, is_pretty_print, convert_types);

    if (values->is_list()) {
      Value current_account_policies_data(Value::Type::DICTIONARY);
      current_account_policies_data.SetKey("id", Value(user_id));
      current_account_policies_data.SetKey("user_id", Value(user_id));
      current_account_policies_data.SetKey("name", Value(user_id));
      current_account_policies_data.SetKey("policies",
                                           std::move(current_account_policies));
      values->GetList().push_back(std::move(current_account_policies_data));
    } else {
      values->SetKey(user_id, std::move(current_account_policies));
    }
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

const LocalizedString kPolicySources[POLICY_SOURCE_COUNT] = {
    {"sourceEnterpriseDefault", IDS_POLICY_SOURCE_ENTERPRISE_DEFAULT},
    {"cloud", IDS_POLICY_SOURCE_CLOUD},
    {"sourceActiveDirectory", IDS_POLICY_SOURCE_ACTIVE_DIRECTORY},
    {"sourceDeviceLocalAccountOverride",
     IDS_POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE},
    {"platform", IDS_POLICY_SOURCE_PLATFORM},
    {"priorityCloud", IDS_POLICY_SOURCE_CLOUD},
    {"merged", IDS_POLICY_SOURCE_MERGED},
};

Value GetAllPolicyValuesAsArray(content::BrowserContext* context,
                                bool with_user_policies,
                                bool convert_values,
                                bool with_device_data,
                                bool is_pretty_print,
                                bool convert_types) {
  Value all_policies(Value::Type::LIST);
  DCHECK(context);

  context = chrome::GetBrowserContextRedirectedInIncognito(context);

  // Add Chrome policy values.
  Value chrome_policies(Value::Type::DICTIONARY);
  GetChromePolicyValues(context, with_user_policies, convert_values,
                        &chrome_policies, is_pretty_print, convert_types);
  Value chrome_policies_data(Value::Type::DICTIONARY);
  chrome_policies_data.SetKey("name", Value("Chrome Policies"));
  chrome_policies_data.SetKey("policies", std::move(chrome_policies));

  all_policies.GetList().push_back(std::move(chrome_policies_data));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Add extension policy values.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(Profile::FromBrowserContext(context));
  if (!registry) {
    LOG(ERROR) << "Can not dump extension policies, no extension registry";
    return all_policies;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  auto* schema_registry_service = profile->GetPolicySchemaRegistryService();
  if (!schema_registry_service || !schema_registry_service->registry()) {
    LOG(ERROR) << "Can not dump extension policies, no schema registry service";
    return all_policies;
  }
  const scoped_refptr<SchemaMap> schema_map =
      schema_registry_service->registry()->schema_map();
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    // Skip this extension if it's not an enterprise extension.
    if (!extension->manifest()->HasPath(
            extensions::manifest_keys::kStorageManagedSchema)) {
      continue;
    }

    Value extension_policies(Value::Type::DICTIONARY);
    PolicyNamespace policy_namespace =
        PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, extension->id());
    PolicyErrorMap empty_error_map;
    GetPolicyValues(
        GetPolicyService(context)->GetPolicies(policy_namespace),
        &empty_error_map, with_user_policies, convert_values,
        GetKnownPolicies(schema_map, policy_namespace, is_pretty_print),
        &extension_policies, is_pretty_print, convert_types);
    Value extension_policies_data(Value::Type::DICTIONARY);
    extension_policies_data.SetKey("name", Value(extension->name()));
    extension_policies_data.SetKey("id", Value(extension->id()));
    extension_policies_data.SetKey("policies", std::move(extension_policies));
    all_policies.GetList().push_back(std::move(extension_policies_data));
  }
#endif

#if defined(OS_CHROMEOS)
  Value device_local_account_policies(Value::Type::DICTIONARY);
  GetDeviceLocalAccountPolicies(convert_values, &all_policies, with_device_data,
                                is_pretty_print, convert_types);
#endif  // defined(OS_CHROMEOS)

  return all_policies;
}

Value GetAllPolicyValuesAsDictionary(content::BrowserContext* context,
                                     bool with_user_policies,
                                     bool convert_values,
                                     bool with_device_data,
                                     bool is_pretty_print,
                                     bool convert_types) {
  Value all_policies(Value::Type::DICTIONARY);
  if (!context) {
    LOG(ERROR) << "Can not dump policies, null context";
    return all_policies;
  }

  context = chrome::GetBrowserContextRedirectedInIncognito(context);

  // Add Chrome policy values.
  Value chrome_policies(Value::Type::DICTIONARY);
  GetChromePolicyValues(context, with_user_policies, convert_values,
                        &chrome_policies, is_pretty_print, convert_types);
  all_policies.SetKey("chromePolicies", std::move(chrome_policies));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Add extension policy values.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(Profile::FromBrowserContext(context));
  if (!registry) {
    LOG(ERROR) << "Can not dump extension policies, no extension registry";
    return all_policies;
  }
  Value extension_values(Value::Type::DICTIONARY);
  Profile* profile = Profile::FromBrowserContext(context);
  auto* schema_registry_service = profile->GetPolicySchemaRegistryService();
  if (!schema_registry_service || !schema_registry_service->registry()) {
    LOG(ERROR) << "Can not dump extension policies, no schema registry service";
    return all_policies;
  }
  const scoped_refptr<SchemaMap> schema_map =
      schema_registry_service->registry()->schema_map();
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    // Skip this extension if it's not an enterprise extension.
    if (!extension->manifest()->HasPath(
            extensions::manifest_keys::kStorageManagedSchema))
      continue;
    Value extension_policies(Value::Type::DICTIONARY);
    PolicyNamespace policy_namespace =
        PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, extension->id());
    PolicyErrorMap empty_error_map;
    GetPolicyValues(
        GetPolicyService(context)->GetPolicies(policy_namespace),
        &empty_error_map, with_user_policies, convert_values,
        GetKnownPolicies(schema_map, policy_namespace, is_pretty_print),
        &extension_policies, is_pretty_print, convert_types);
    extension_values.SetKey(extension->id(), std::move(extension_policies));
  }
  all_policies.SetKey("extensionPolicies", std::move(extension_values));
#endif

#if defined(OS_CHROMEOS)
  Value device_local_account_policies(Value::Type::DICTIONARY);
  GetDeviceLocalAccountPolicies(convert_values, &device_local_account_policies,
                                with_device_data, is_pretty_print,
                                convert_types);
  all_policies.SetKey("deviceLocalAccountPolicies",
                      std::move(device_local_account_policies));
#endif  // defined(OS_CHROMEOS)

  if (with_device_data)
    FillIdentityFields(&all_policies);

  return all_policies;
}

#if defined(OS_CHROMEOS)
void FillIdentityFieldsFromPolicy(const em::PolicyData* policy,
                                  Value* policy_dump) {
  if (!policy) {
    return;
  }
  DCHECK(policy_dump);

  if (policy->has_device_id())
    policy_dump->SetKey("client_id", Value(policy->device_id()));

  if (policy->has_annotated_location())
    policy_dump->SetKey("device_location", Value(policy->annotated_location()));

  if (policy->has_annotated_asset_id())
    policy_dump->SetKey("asset_id", Value(policy->annotated_asset_id()));

  if (policy->has_display_domain())
    policy_dump->SetKey("display_domain", Value(policy->display_domain()));

  if (policy->has_machine_name())
    policy_dump->SetKey("machine_name", Value(policy->machine_name()));
}
#endif  // defined(OS_CHROMEOS)

void FillIdentityFields(Value* policy_dump) {
#if defined(OS_CHROMEOS)
  DCHECK(policy_dump);
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector) {
    LOG(ERROR) << "Can not dump identity fields, no policy connector";
    return;
  }
  if (connector->IsEnterpriseManaged()) {
    policy_dump->SetKey("enrollment_domain",
                        Value(connector->GetEnterpriseEnrollmentDomain()));

    if (connector->IsActiveDirectoryManaged()) {
      FillIdentityFieldsFromPolicy(
          connector->GetDeviceActiveDirectoryPolicyManager()->store()->policy(),
          policy_dump);
    }

    if (connector->IsCloudManaged()) {
      FillIdentityFieldsFromPolicy(
          connector->GetDeviceCloudPolicyManager()->device_store()->policy(),
          policy_dump);
    }
  }
#endif  // defined(OS_CHROMEOS)
}

std::string GetAllPolicyValuesAsJSON(content::BrowserContext* context,
                                     bool with_user_policies,
                                     bool with_device_data,
                                     bool is_pretty_print) {
  Value all_policies = GetAllPolicyValuesAsDictionary(
      context, with_user_policies, false /* convert_values */, with_device_data,
      is_pretty_print, true /* convert_types */);

  return DictionaryToJSONString(all_policies, is_pretty_print);
}

}  // namespace policy
