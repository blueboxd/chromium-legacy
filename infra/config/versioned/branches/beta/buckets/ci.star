load('//versioned/vars/ci.star', 'vars')
vars.bucket.set('ci-beta')
vars.poller.set('beta-gitiles-trigger')

load('//lib/builders.star', 'defaults')
defaults.pool.set('luci.chromium.ci')

load('//versioned/milestones.star', milestone='beta')
exec('//versioned/milestones/%s/buckets/ci.star' % milestone)
