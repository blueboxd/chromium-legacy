# CookiesTreeModel

A CookiesTreeModel is instantiated in multiple places in Chrome:

* "All cookies and site data" (chrome://settings/siteData)
* "All sites" (chrome://settings/content/all)
* "Cookies in use" display off the origin chip in the infobar

## BrowsingDataXYZHelper

Instances of this type are used to fully populate a CookiesTreeModel
with full details (e.g. origin/size/modified) for different storage
types, e.g. to report storage used by all origins.

When StartFetching is called, a call is made into the relevant storage
context to enumerate usage info - usually, a set of tuples of (origin,
size, last modified). The CookiesTreeModel assembles this into the
tree of nodes used to populate UI.

Some UI also uses this to delete origin data, which again calls into
the storage context.

## CannedBrowsingDataXYZHelper

Subclass of the above. These are created to sparsely populate a
CookiesTreeModel on demand by LocalSharedObjectContainer, with only
some details (e.g. full details for cookies, but only the usage of
other storage typess).

* PageSpecificContentSettings is notified on storage access/blocked.
* It calls into the "canned" helper instance for the storage type.
* The "canned" instance records necessary "pending" info about the access.
* On demand, the "pending" info is used to populate a CookiesTreeModel.

This "pending" info only needs to record the origin for most storage
types.
