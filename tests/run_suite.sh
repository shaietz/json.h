#!/bin/sh
# Run JSONTestSuite's test_parsing cases against a runner binary.
#   y_*  must be accepted (exit 0)
#   n_*  must be rejected (exit 1)
#   i_*  either is allowed, but must not crash
# Usage: run_suite.sh RUNNER DIR [-v]    (-v lists how each i_ case went)

runner=$1
dir=$2
verbose=$3

if [ ! -x "$runner" ] || [ ! -d "$dir" ]; then
    echo "usage: $0 RUNNER DIR [-v]" >&2
    exit 2
fi

pass=0 fail=0 i_acc=0 i_rej=0

for f in "$dir"/*.json; do
    name=$(basename "$f")
    "$runner" "$f" >/dev/null 2>&1
    rc=$?
    case "$name" in
        y_*) if [ $rc -eq 0 ]; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL should accept (rc=$rc): $name"; fi ;;
        n_*) if [ $rc -eq 1 ]; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL should reject (rc=$rc): $name"; fi ;;
        i_*)
            if [ $rc -gt 1 ]; then
                fail=$((fail+1)); echo "FAIL crashed (rc=$rc): $name"
            else
                pass=$((pass+1))
                if [ $rc -eq 0 ]; then i_acc=$((i_acc+1)); [ "$verbose" = -v ] && echo "  accept  $name"
                else                   i_rej=$((i_rej+1)); [ "$verbose" = -v ] && echo "  reject  $name"
                fi
            fi ;;
    esac
done

echo "JSONTestSuite: $pass passed, $fail failed ($i_acc i_ accepted, $i_rej i_ rejected)"
[ $fail -eq 0 ]