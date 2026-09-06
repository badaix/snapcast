#!/bin/bash
# MPTCP failover measurement lab for snapcast.
#
# Creates an ephemeral user+network namespace (unshare -Urn) in which this
# script has root rights on a private network stack. Inside, snapserver and
# snapclient run in two separate network namespaces, connected by two
# parallel veth links (10.0.1.0/24 on path A, 10.0.2.0/24 on path B). A
# fake (silent) stream is fed to the server, the client consumes it into a
# null player. Once audio is flowing, path A is shut down silently
# ("ip link set down") and the script measures when the full stream rate is
# sustained on path B again.
#
# Environment knobs:
#   SCHEDULER        MPTCP packet scheduler (only "default" on most kernels)
#   BACKUP           1: mark the second subflow as backup (MP_PRIO)
#   STALE_LOSS_CNT   net.mptcp.stale_loss_cnt value (default: kernel default)
#   DUAL_HOMED       server (default): server announces its second address,
#                    client creates the second subflow
#                    client: client announces its second address (single-homed
#                    server), server creates the second subflow
#   MPTCP            0: plain TCP baseline run (no MPTCP on server/client)
#   BIN_DIR          directory containing snapserver/snapclient (default: ../../bin)
#
# Prints a machine readable summary line:
#   RESULT gap=<s|n/a> recovery=<s|n/a> overhead_b_pkts=<n>

set -u
ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BIN_DIR="${BIN_DIR:-$ROOT_DIR/bin}"
SCHEDULER="${SCHEDULER:-default}"
BACKUP="${BACKUP:-0}"
STALE_LOSS_CNT="${STALE_LOSS_CNT:-}"
DUAL_HOMED="${DUAL_HOMED:-server}"
MPTCP="${MPTCP:-1}"

if [ ! -x "$BIN_DIR/snapserver" ] || [ ! -x "$BIN_DIR/snapclient" ]; then
    echo "ERROR: snapserver/snapclient not found in $BIN_DIR" >&2
    exit 1
fi

if [ "${LAB_INNER:-}" != "1" ]; then
    if [ "$(id -u)" = "0" ]; then
        # already root (e.g. CI with sudo): a plain network namespace
        # suffices, no user namespace needed. It also isolates every run
        # from the host netns (listener ports, veth names).
        exec unshare -n env LAB_INNER=1 SCHEDULER="$SCHEDULER" BACKUP="$BACKUP" STALE_LOSS_CNT="$STALE_LOSS_CNT" DUAL_HOMED="$DUAL_HOMED" \
            MPTCP="$MPTCP" BIN_DIR="$BIN_DIR" bash "$0"
    fi
    exec unshare -Urn env LAB_INNER=1 SCHEDULER="$SCHEDULER" BACKUP="$BACKUP" STALE_LOSS_CNT="$STALE_LOSS_CNT" DUAL_HOMED="$DUAL_HOMED" \
        MPTCP="$MPTCP" BIN_DIR="$BIN_DIR" bash "$0"
fi

# From here on we are root in a private network namespace (lab netns = server side).
set -eu

SERVER_PID=""
CLIENT_PID=""
TP_A=""
TP_B=""
FEEDER_PID=""
CLINS_PID=""
MON_PID=""
cleanup() {
    kill "$CLIENT_PID" "$SERVER_PID" "$TP_A" "$TP_B" "$FEEDER_PID" "$CLINS_PID" "$MON_PID" 2>/dev/null || true
    pkill -f "tcp[d]ump -Z root" 2>/dev/null || true
    pkill -f "ip mp[tcp] monitor" 2>/dev/null || true
    pkill -f "sleep [3]00" 2>/dev/null || true
}
trap cleanup EXIT

rm -f /tmp/snapfifo.lab /tmp/pkt_a.tsv /tmp/pkt_b.tsv /tmp/snapserver.lab.log /tmp/snapclient.lab.log \
    /tmp/clins.ready /tmp/td_a.err /tmp/td_b.err /tmp/mptcp_mon.log

# --- links: veth_sX stay in the lab (server) netns, veth_cX move to the client netns ---
ip link add veth_s1 type veth peer name veth_c1
ip link add veth_s2 type veth peer name veth_c2
ip link set lo up
ip addr add 10.0.1.1/24 dev veth_s1
ip link set veth_s1 up
ip link set veth_s2 up
if [ "$DUAL_HOMED" = "client" ]; then
    # single-homed server: veth_s2 has no address in the client subnet, path B
    # is reached via an explicit route (source selection falls back to 10.0.1.1)
    ip route add 10.0.2.0/24 dev veth_s2
else
    ip addr add 10.0.2.1/24 dev veth_s2
fi

# --- client network namespace ---
unshare -n sh -c 'touch /tmp/clins.ready; exec sleep 300' >/dev/null 2>&1 &
CLINS_PID=$!
for _ in $(seq 1 50); do
    [ -e /tmp/clins.ready ] && break
    sleep 0.1
done
[ -e /tmp/clins.ready ] || { echo "ERROR: client netns did not start" >&2; exit 1; }

ip link set veth_c1 netns "$CLINS_PID"
ip link set veth_c2 netns "$CLINS_PID"
nsenter -t "$CLINS_PID" -n sh -c '
    set -eu
    ip link set lo up
    ip addr add 10.0.1.2/24 dev veth_c1
    ip addr add 10.0.2.2/24 dev veth_c2
    ip link set veth_c1 up
    ip link set veth_c2 up
'

# --- MPTCP configuration ---
MPTCP_CFG=""
CLIENT_MPTCP=""
if [ "$MPTCP" = "1" ]; then
    MPTCP_CFG="--tcp-streaming.mptcp true"
    CLIENT_MPTCP="--mptcp true"
    # trigger per-netns MPTCP init so the sysctls exist
    python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_STREAM, 262); s.close()"
    echo "$SCHEDULER" > /proc/sys/net/mptcp/scheduler 2>/dev/null || true
    if [ -n "$STALE_LOSS_CNT" ]; then
        echo "$STALE_LOSS_CNT" > /proc/sys/net/mptcp/stale_loss_cnt
    fi
    echo "scheduler: $(cat /proc/sys/net/mptcp/scheduler), backup: $BACKUP, stale_loss_cnt: $(cat /proc/sys/net/mptcp/stale_loss_cnt)"
    ip mptcp limits set subflows 2 add_addr_accepted 2

    nsenter -t "$CLINS_PID" -n sh -c '
        set -eu
        python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_STREAM, 262); s.close()"
        echo "$1" > /proc/sys/net/mptcp/scheduler 2>/dev/null || true
        ip mptcp limits set subflows 2 add_addr_accepted 2
        # allow incoming MP_JOIN to the initial port (needed when the server
        # creates a subflow to the announced second client address)
        echo 1 > /proc/sys/net/mptcp/allow_join_initial_addr_port 2>/dev/null || true
    ' -- "$SCHEDULER"

    BACKUP_FLAG=""
    if [ "$BACKUP" = "1" ]; then BACKUP_FLAG="backup"; fi
    if [ "$DUAL_HOMED" = "client" ]; then
        # server is single-homed, client announces its second address
        ip mptcp endpoint add 10.0.1.1 dev veth_s1 subflow $BACKUP_FLAG
        nsenter -t "$CLINS_PID" -n ip mptcp endpoint add 10.0.2.2 dev veth_c2 signal
    else
        # server announces its second address, client creates the second subflow
        ip mptcp endpoint add 10.0.2.1 dev veth_s2 signal
        nsenter -t "$CLINS_PID" -n ip mptcp endpoint add 10.0.2.2 dev veth_c2 subflow $BACKUP_FLAG
    fi
fi

# --- endless raw PCM silence (48kHz, 16bit, stereo => 1920 bytes per 10ms) ---
mkfifo /tmp/snapfifo.lab
dd if=/dev/zero of=/tmp/snapfifo.lab bs=1920 >/dev/null 2>&1 &
FEEDER_PID=$!

# --- start snapcast (codec=pcm: core codec, no external codec libs needed) ---
$BIN_DIR/snapserver -c /dev/null --logging.sink stdout $MPTCP_CFG \
    --stream.source 'pipe:///tmp/snapfifo.lab?name=test&sampleformat=48000:16:2&codec=pcm' > /tmp/snapserver.lab.log 2>&1 &
SERVER_PID=$!
sleep 1
nsenter -t "$CLINS_PID" -n $BIN_DIR/snapclient tcp://10.0.1.1:1704 $CLIENT_MPTCP --player 'file:filename=null' > /tmp/snapclient.lab.log 2>&1 &
CLIENT_PID=$!

# --- capture server->client data packets on both paths (client netns side) ---
nsenter -t "$CLINS_PID" -n tcpdump -Z root -q -tt -l -i veth_c1 "src host 10.0.1.1 and tcp port 1704 and greater 100" > /tmp/pkt_a.tsv 2>/tmp/td_a.err &
TP_A=$!
# note: no src filter on path B - the fullmesh path manager may create a
# subflow with the server's path-A source address routed over path B
nsenter -t "$CLINS_PID" -n tcpdump -Z root -q -tt -l -i veth_c2 "tcp port 1704 and greater 100" > /tmp/pkt_b.tsv 2>/tmp/td_b.err &
TP_B=$!
if [ "$MPTCP" = "1" ]; then
    ip mptcp monitor > /tmp/mptcp_mon.log 2>&1 &
    MON_PID=$!
fi

sleep 3

if [ ! -s /tmp/pkt_a.tsv ]; then
    echo "ERROR: no audio traffic on path A" >&2
    cat /tmp/snapserver.lab.log >&2
    cat /tmp/snapclient.lab.log >&2
    cat /tmp/td_a.err /tmp/td_b.err >&2 2>/dev/null
    exit 1
fi

echo "--- sockets before kill (server side) ---"
ss -tin "( sport = :1704 )" 2>/dev/null | head -3 || true
echo "--- sockets before kill (client side: Recv-Q must not grow => client consumes) ---"
nsenter -t "$CLINS_PID" -n ss -tin "( dport = :1704 )" 2>/dev/null | head -3 || true

# --- the outage: silently kill path A on the client side ---
KILL_TS=$(date +%s.%N)
nsenter -t "$CLINS_PID" -n ip link set veth_c1 down

sleep 5

if kill -0 "$CLIENT_PID" 2>/dev/null; then
    echo "client connected after outage: yes"
else
    echo "client connected after outage: NO (disconnected)"
fi
echo "client log errors: $(grep -cE 'Error' /tmp/snapclient.lab.log || true)"
if [ "$MPTCP" = "1" ]; then
    echo "--- mptcp events ---"
    grep -E "CREATED|DELETED|MP_FAIL|FASTCLOSE|MP_PRIO|ESTABLISHED|ANNOUNCED|SUB" /tmp/mptcp_mon.log 2>/dev/null | head -10 || true
fi

python3 - "$KILL_TS" <<'PYEOF'
import sys
kill_ts = float(sys.argv[1])
WINDOW = 0.3
TARGET_RATE_FRACTION = 0.9


def packets(path):
    ts = []
    try:
        with open(path) as f:
            for line in f:
                parts = line.split()
                try:
                    ts.append(float(parts[0]))
                except (ValueError, IndexError):
                    pass
    except OSError:
        pass
    return ts


a = packets('/tmp/pkt_a.tsv')
b = packets('/tmp/pkt_b.tsv')
a_before = [t for t in a if t < kill_ts]
b_before = [t for t in b if t < kill_ts]
b_after = [t for t in b if t >= kill_ts]
last_a = a[-1] if a else None
first_b = b_after[0] if b_after else None
gap = (first_b - last_a) if (first_b is not None and last_a is not None) else None

# pre-kill total stream rate (packets/s, last second before the kill)
recent = [t for t in a_before + b_before if t >= kill_ts - 1.0]
rate = len(recent) / 1.0

# sustained recovery: first time t >= kill_ts where path B carries >= 90% of
# the pre-kill total rate over a 300 ms window (the full stream, not
# sporadic packets)
recovery = None
if b_after:
    target = max(3, int(TARGET_RATE_FRACTION * rate * WINDOW))
    for t in b_after:
        count = sum(1 for x in b_after if t <= x < t + WINDOW)
        if count >= target:
            recovery = t - kill_ts
            break

print(f"packets on A before kill: {len(a_before)}")
print(f"packets on B before kill (duplicate traffic): {len(b_before)}")
print(f"total rate before kill: {rate:.0f} pkts/s")
print(f"last packet on A: {(last_a - kill_ts) * 1000:+.1f} ms rel. to kill" if last_a is not None else "last packet on A: n/a")
print(f"first packet on B: {(first_b - kill_ts) * 1000:+.1f} ms rel. to kill" if first_b is not None else "first packet on B: n/a")
print(f"audio gap (first data): {gap * 1000:.1f} ms" if gap is not None else "audio gap (first data): n/a (no failover)")
print(f"full rate resumed on B: {recovery * 1000:.1f} ms" if recovery is not None else "full rate resumed on B: n/a")
if gap is not None:
    rec = f"{recovery:.3f}" if recovery is not None else "n/a"
    print(f"RESULT gap={gap:.3f} recovery={rec} overhead_b_pkts={len(b_before)}")
else:
    print(f"RESULT gap=n/a recovery=n/a overhead_b_pkts={len(b_before)}")
PYEOF
