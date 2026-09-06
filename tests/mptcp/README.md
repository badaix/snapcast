# MPTCP failover lab

Reproducible measurement of how long a snapcast audio stream stalls when one
network path dies silently, using Linux Multipath TCP (MPTCP).

## How it works

`lab.sh` re-execs itself inside an ephemeral user+network namespace
(`unshare -Urn`), where it has root rights on a private network stack.
Snapserver and snapclient run in two separate network namespaces, connected
by two parallel veth links:

```
netns "server" ── veth path A (10.0.1.0/24) ──┐
               ── veth path B (10.0.2.0/24) ──┼── netns "client"
```

The server announces its second address (`ip mptcp endpoint ... signal`),
the client creates a second subflow (`... subflow [backup]`), so the MPTCP
connection uses both links. After 3 s of audio (raw PCM silence via a pipe
source, `codec=pcm`), path A is shut down with `ip link set veth_c1 down`
(a silent failure: no FIN/RST). `tcpdump` on both paths measures when audio
data resumes on path B — that is the failover gap. A plain TCP baseline run
(`MPTCP=0`) shows the same scenario without MPTCP.

Every run happens in a fresh namespace, so the test is fully repeatable and
leaves no state behind.

## Prerequisites

- util-linux (`unshare`), iproute2 with `ip mptcp` support
- `tcpdump`, `python3`
- Linux kernel with MPTCP (`CONFIG_MPTCP`), built `snapserver`/`snapclient`
  binaries in the repository's `bin/` directory (env `BIN_DIR` overrides)

## Usage

```sh
# single run (defaults)
tests/mptcp/lab.sh

# knob variations (BACKUP=1, stale_loss_cnt=1)
BACKUP=1 STALE_LOSS_CNT=1 tests/mptcp/lab.sh

# full optimization sweep over backup flag and stale_loss_cnt + TCP baseline
tests/mptcp/optimize.sh
```

`lab.sh` prints a machine readable
`RESULT gap=<s|n/a> recovery=<s|n/a> overhead_b_pkts=<n>` line, where `gap`
is the time until the first data packet on the surviving path and `recovery`
the time until the full stream rate is sustained there again. `optimize.sh`
aggregates a table and picks the minimum recovery time.

## Results (kernel 7.0.14-12-pve, MPTCP scheduler: default)

Measured with `optimize.sh` (outage = silent loss of path A):

| config               | first data on B | full rate on B |
|----------------------|-----------------|----------------|
| MPTCP, `backup=0`    | 25–35 ms        | 15–18 ms       |
| MPTCP, `backup=1`    | 227–363 ms      | 220–360 ms     |
| plain TCP (baseline) | never           | never          |

- With the default configuration the stream switches paths within ~1–2
  audio chunks (20 ms each). The snapclient buffer (1 s) absorbs this
  completely: no audible interruption, no reconnect (client log: 0 errors,
  still connected after the outage in every run).
- `net.mptcp.stale_loss_cnt` does not improve the failover time (kernel
  default is fine); only the `default` scheduler is compiled into most
  distro kernels, so `SCHEDULER` has no alternatives there.
- Plain TCP stalls indefinitely when a path dies silently - the connection
  only recovers when the link comes back (or TCP gives up after ~15 min).

## DUAL_HOMED modes

- `DUAL_HOMED=server` (default): the server announces its second address,
  the client creates the second subflow. Verified working.
- `DUAL_HOMED=client` (experimental): emulates a single-homed server whose
  client announces its second address; the server should create the second
  subflow. On kernel 7.0 this does not work yet - the kernel path manager
  does not create server-initiated subflows in this setup. The lab sets
  `net.mptcp.allow_join_initial_addr_port=1` in the client netns (needed on
  kernels >= 6.13 to accept MP_JOIN to the initial port at all).
