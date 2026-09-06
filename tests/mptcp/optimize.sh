#!/bin/bash
# Varies MPTCP configuration knobs, measures the failover gap after a silent
# link failure with tests/mptcp/lab.sh and reports the best configuration.
#
# Compared knobs:
#   - second subflow marked as backup (MP_PRIO) or not
#   - net.mptcp.stale_loss_cnt (kernel default 4 vs 1 vs 0)
# Plus a plain TCP baseline run (MPTCP=0).

set -u
LAB="$(cd "$(dirname "$0")" && pwd)/lab.sh"

printf "%-4s %-8s %-12s %-12s %s\n" "bkup" "stale" "gap" "recovery" "detail"
BEST=""
BEST_KEY=""

run() {
    local backup=$1
    local stale=$2
    local out gap recovery dup key
    out=$(BACKUP="$backup" STALE_LOSS_CNT="$stale" "$LAB" 2>&1)
    gap=$(echo "$out" | sed -n 's/^RESULT gap=\([^ ]*\) .*/\1/p')
    recovery=$(echo "$out" | sed -n 's/^RESULT gap=[^ ]* recovery=\([^ ]*\) .*/\1/p')
    dup=$(echo "$out" | sed -n 's/^RESULT gap=[^ ]* recovery=[^ ]* overhead_b_pkts=\(.*\)/\1/p')
    printf "%-4s %-8s %-12s %-12s %s\n" "$backup" "${stale:-def}" "${gap:-?}" "${recovery:-?}" \
        "$(echo "$out" | grep -E 'audio gap|full rate resumed' | tr '\n' ' ')"
    if [ -n "$recovery" ] && [ "$recovery" != "n/a" ]; then
        key="bkup=$backup stale=${stale:-def} recovery=$recovery"
        if [ -z "$BEST_KEY" ] || awk "BEGIN{exit !($recovery < $BEST_GAP)}"; then
            BEST_GAP=$recovery
            BEST_KEY=$key
        fi
    fi
}

echo "--- MPTCP runs ---"
for BACKUP in 0 1; do
    for STALE in "" 4 1 0; do
        run "$BACKUP" "$STALE"
    done
done

echo "---"
echo "plain TCP baseline (no MPTCP):"
MPTCP=0 run "-" ""

echo "---"
if [ -n "$BEST_KEY" ]; then
    echo "best config: $BEST_KEY"
else
    echo "best config: none (no failover measured)"
fi
