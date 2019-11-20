load('//versioned/vars/try.star', 'vars')
vars.bucket.set('try-beta')
vars.cq_group.set('cq-beta')

load('//lib/builders.star', 'defaults')
defaults.pool.set('luci.chromium.try')

load('//versioned/milestones.star', milestone='beta')
exec('//versioned/milestones/%s/buckets/try.star' % milestone)
