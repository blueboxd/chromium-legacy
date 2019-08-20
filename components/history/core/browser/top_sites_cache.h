// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_CACHE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_CACHE_H_

#include <stddef.h>

#include <map>
#include <utility>

#include "base/macros.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_utils.h"
#include "url/gurl.h"

namespace history {

// TopSitesCache caches the list of top sites for TopSites.
class TopSitesCache {
 public:
  TopSitesCache();
  ~TopSitesCache();

  // Set the top sites.
  void SetTopSites(const MostVisitedURLList& top_sites);
  const MostVisitedURLList& top_sites() const { return top_sites_; }

  // Returns the canonical URL for |url|. If not found, returns |url|.
  const GURL& GetCanonicalURL(const GURL& url) const;

  // Returns true if |url| is known.
  bool IsKnownURL(const GURL& url) const;

  // Returns the index into |top_sites_| for |url|.
  size_t GetURLIndex(const GURL& url) const;

  // Returns the number of URLs in the cache.
  size_t GetNumURLs() const;

 private:
  // The entries in CanonicalURLs, see CanonicalURLs for details. The second
  // argument gives the index of the URL into MostVisitedURLs redirects.
  using CanonicalURLEntry = std::pair<MostVisitedURL*, size_t>;

  // Comparator used for CanonicalURLs.
  class CanonicalURLComparator {
   public:
    bool operator()(const CanonicalURLEntry& e1,
                    const CanonicalURLEntry& e2) const {
      return CanonicalURLStringCompare(e1.first->redirects[e1.second].spec(),
                                       e2.first->redirects[e2.second].spec());
    }
  };

  // Creates the object needed to form std::map queries into |canonical_urls_|,
  // wrapping all required temporary data to allow inlining.
  class CanonicalURLQuery {
   public:
    explicit CanonicalURLQuery(const GURL& url);
    ~CanonicalURLQuery();
    const CanonicalURLEntry& entry() { return entry_; }

   private:
    MostVisitedURL most_visited_url_;
    CanonicalURLEntry entry_;
  };

  // This is used to map from redirect url to the MostVisitedURL the redirect is
  // from. Ideally this would be map<GURL, size_t> (second param indexing into
  // top_sites_), but this results in duplicating all redirect urls. As some
  // sites have a lot of redirects, we instead use the MostVisitedURL* and the
  // index of the redirect as the key, and the index into top_sites_ as the
  // value. This way we aren't duplicating GURLs. CanonicalURLComparator
  // enforces the ordering as if we were using GURLs.
  using CanonicalURLs =
      std::map<CanonicalURLEntry, size_t, CanonicalURLComparator>;

  // Generates the set of canonical urls from |top_sites_|.
  void GenerateCanonicalURLs();

  // Stores a set of redirects. This is used by GenerateCanonicalURLs.
  void StoreRedirectChain(const RedirectList& redirects, size_t destination);

  // Returns the iterator into |canonical_urls_| for the |url|.
  CanonicalURLs::const_iterator GetCanonicalURLsIterator(const GURL& url) const;

  // Returns the GURL corresponding to an iterator in |canonical_urls_|.
  const GURL& GetURLFromIterator(CanonicalURLs::const_iterator it) const;

  // The list of top sites.
  MostVisitedURLList top_sites_;

  // Generated from the redirects to and from the most visited pages. See
  // description above CanonicalURLs for details.
  CanonicalURLs canonical_urls_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesCache);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_CACHE_H_
