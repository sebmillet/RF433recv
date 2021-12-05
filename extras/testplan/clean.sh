#!/usr/bin/bash

set -euo pipefail

cd one

for d in [0-9][0-9]; do
    rm -f "${d}"/tmpout*.txt
done

