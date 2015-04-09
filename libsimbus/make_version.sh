#!/bin/sh

# The version is mostly constructed from the git tag libcnan-*, with
# indications of how far from the most recent tag we've come.
vers=`git describe --tags --long --always --dirty`

echo $vers
