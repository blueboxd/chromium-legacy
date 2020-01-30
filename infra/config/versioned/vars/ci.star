load('//lib/bucket.star', bucket_var='var')

vars = struct(
    bucket = bucket_var(default = 'ci'),
    poller = lucicfg.var(default = 'master-gitiles-trigger'),
    poller_refs_regexp = lucicfg.var(default = 'refs/head/master'),
)
