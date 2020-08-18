// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/lookup_affiliation_response_parser.h"

namespace password_manager {

bool ParseLookupAffiliationResponse(
    const std::vector<FacetURI>& requested_facet_uris,
    const affiliation_pb::LookupAffiliationResponse& response,
    AffiliationFetcherDelegate::Result* result) {
  result->affiliations.reserve(requested_facet_uris.size());

  std::map<FacetURI, size_t> facet_uri_to_class_index;
  for (int i = 0; i < response.affiliation_size(); ++i) {
    const affiliation_pb::Affiliation& equivalence_class(
        response.affiliation(i));

    AffiliatedFacets affiliated_facets;
    for (int j = 0; j < equivalence_class.facet_size(); ++j) {
      const affiliation_pb::Facet& facet(equivalence_class.facet(j));
      const std::string& uri_spec(facet.id());
      FacetURI uri = FacetURI::FromPotentiallyInvalidSpec(uri_spec);
      // Ignore potential future kinds of facet URIs (e.g. for new platforms).
      if (!uri.is_valid())
        continue;
      affiliated_facets.push_back(
          {uri, FacetBrandingInfo{facet.branding_info().name(),
                                  GURL(facet.branding_info().icon_url())}});
    }

    // Be lenient and ignore empty (after filtering) equivalence classes.
    if (affiliated_facets.empty())
      continue;

    // Ignore equivalence classes that are duplicates of earlier ones. However,
    // fail in the case of a partial overlap, which violates the invariant that
    // affiliations must form an equivalence relation.
    for (const Facet& facet : affiliated_facets) {
      if (!facet_uri_to_class_index.count(facet.uri))
        facet_uri_to_class_index[facet.uri] = result->affiliations.size();
      if (facet_uri_to_class_index[facet.uri] !=
          facet_uri_to_class_index[affiliated_facets[0].uri]) {
        return false;
      }
    }

    // Filter out duplicate equivalence classes in the response.
    if (facet_uri_to_class_index[affiliated_facets[0].uri] ==
        result->affiliations.size()) {
      result->affiliations.push_back(affiliated_facets);
    }
  }

  // Synthesize an equivalence class (of size one) for each facet that did not
  // appear in the server response due to not being affiliated with any others.
  for (const FacetURI& uri : requested_facet_uris) {
    if (!facet_uri_to_class_index.count(uri))
      result->affiliations.push_back({{uri}});
  }

  return true;
}

}  // namespace password_manager
