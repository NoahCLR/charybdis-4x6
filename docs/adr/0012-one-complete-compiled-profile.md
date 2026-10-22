# One complete compiled profile, read by domain

Cited as D-F12.

The compiled defaults are a complete profile holding every enabled domain, and
reads seek a domain by its offset, so no domain depends on being last and one
stream defines both checksums and readback. Each domain has one authored
writer, whose encoded result warms the same caches a stored profile does, so
defaults and saved profiles are read one way. Changing the compiled defaults
changes their digest, and a saved profile bound to the old digest gives way to
the compiled defaults (D-F10).
