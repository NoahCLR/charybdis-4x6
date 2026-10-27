# A registry row names its domain module

Cited as D-F13.

Each registry row names its domain's module beside its ID and version; its
index is its position and its mask bit follows, so neither can drift from the
row order. The validator, the compiled defaults and the compiled writer bind
each row's operations by that name in switches generated from the rows: a row
without its module's validator or encoder does not compile, and every call
stays a direct edge for the reviewed stack paths, with no function-pointer
tables. Each domain accepts exactly one version, so no codec takes a version.
