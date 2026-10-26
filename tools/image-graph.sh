#!/usr/bin/env bash
# image-graph.sh <branch> --- the graph an ok image of this package's tests is
# built against, from the same-named branch of each repository: the C++
# run-time (openkal-llvm-runtime) and the stack beneath it, put in place by that
# repository's own tools/branch-graph.sh, and this manifest pointed at it.
#
# Until those packages are published their versions resolve from no index, so
# the trees are named by path; the manifest that is committed names them by
# their branches, and after the release by their versions. The specification is
# the tree the run-time's graph uses, so the graph names one openkal.
set -euo pipefail
branch="${1:?usage: image-graph.sh <branch>}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rt="$here/.runtime"
rm -rf "$rt"
git clone --quiet --branch "$branch" https://github.com/mcpplibs/openkal-llvm-runtime.git "$rt"
( cd "$rt" && bash tools/branch-graph.sh "$branch" )
native() { if command -v cygpath > /dev/null 2>&1; then cygpath -m "$1"; else printf '%s\n' "$1"; fi; }
sed -e "s|^openkal-llvm-runtime = .*\$|openkal-llvm-runtime = { path = \"$(native "$rt")\" }|" \
    -e "s|^openkal = .*\$|openkal = { path = \"$(native "$rt/.spec")\" }|" \
    "$here/mcpp.toml" > "$here/mcpp.toml.next"
mv "$here/mcpp.toml.next" "$here/mcpp.toml"
grep -nE '^(openkal|openkal-llvm-runtime) = ' "$here/mcpp.toml"
