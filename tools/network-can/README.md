# Network NMEA2000 bridge

`raynmea_bridge.py` needs Python 3.9+, the Linux iproute2 `ip` utility, and the SocketCAN interface already
configured by its owner. It never configures bitrate, link state or startup services.
The application and bridge use RN2K/1, not a second PGN codec.

## Install and run

Copy this directory to a private Linux directory. No pip packages are needed.
Create a random key locally without printing it:

```sh
umask 077
python3 -c 'import secrets; open("access.key","w").write(secrets.token_hex(32))'
python3 raynmea_bridge.py --interface can0 --bind 192.0.2.10 --port 29501 \
  --key-file access.key --allow-client 192.0.2.20/32
```

Replace addresses with your own. Repeat `--allow-client` for additional client
addresses. Default bind is loopback. Omitting the key file makes the server RX-only.
Protect the key file with mode 0600 and its directory with mode 0700. Share the key
privately with authorized simulator users. The HMAC handshake does not send the
key, but traffic is unencrypted: use an SSH tunnel on an untrusted network, and do
not expose the listener to the Internet. For a tunnel, bind the bridge to 127.0.0.1,
allow 127.0.0.1/32, and use `ssh -N -L 29501:127.0.0.1:29501 user@linux-host`;
configure the simulator for 127.0.0.1:29501. IP restrictions apply to RX as well as TX.

In **Setup > NMEA2000**, select **Network NMEA2000**, enable the interface, and enter:

- Host/IP, TCP port, and the exact remote interface name.
- RX-only or RX/TX capability. This capability does not authorize Sending.
- A distinct unique device number for each computer; source-address arbitration
  remains with the existing NMEA2000 core. A duplicate NAME observed from
  another participant revokes that active client's TX with an explicit error.
- The TX access key, saved through the existing OS credential store.

Save while stopped, then use **Connect / reconnect saved network configuration**.
Every connection starts RX-only. **Enable Sending** authorizes instrument TX;
**Stop Sending/F 12**, mode/configuration changes and disconnect revoke it.
Chartplotter **Discover/Refresh/Retrieve** instead authorizes only a bounded route
service session. Clean completion or cancellation returns to RX-only.

A stopped connection never automatically reauthorizes TX. After connection failure,
reconnect explicitly. No automatic bridge service is installed by this utility.
Stopping the bridge closes its client sessions without changing CAN configuration.

## Virtual bus

The same bridge works with an owner-created Linux `vcan0`:

```sh
python3 raynmea_bridge.py --interface vcan0 --key-file access.key
```

Where vcan is unavailable, an isolated in-process raw-frame hub uses the same wire
protocol and has **no CAN socket**:

```sh
python3 raynmea_bridge.py --virtual --interface lab --key-file access.key
```

Use separate ports for physical and virtual listeners. The client's interface name
must match the server greeting; a mismatch fails before a node can transmit.

## Protocol and bounds

RN2K/1 is newline-delimited UTF-8 JSON, <=1024 bytes per command. TCP preserves frame
order. Server greeting: `kind=hello, version=1, interface, client, nonce`.
Client authorization: `kind=auth, interface, mode=rx|tx|service, proof`.
For TX/service the proof is hexadecimal HMAC-SHA256 with the shared key over:
`nonce|client|mode|interface`. A fresh connection has a fresh challenge; reauthorization
requires reconnect. Server replies `kind=ready, mode, nonce`.

Frames use `kind=frame, id, dlc, data`. `id` is the uint 32 Linux CAN identifier including
EFF/RTR flags; `dlc` is 0..8 and `data` contains exactly that many hexadecimal bytes.
Server adds monotonically increasing `seq`, assigned `origin`, and `timestamp_ns`.
The GUI accepts extended Classic-CAN data frames into its existing protocol core;
standard/RTR frames remain bridge-compatible but do not become NMEA2000 messages.
CAN FD and outbound error injection are unsupported. Received CAN errors revoke TX.

The bridge sends each network-origin frame to SocketCAN once, then forwards it to
other clients once. Its CAN socket disables receive-own-messages, avoiding a second
kernel echo. Bus-origin frames are forwarded once. Frames are never content-deduplicated
because identical repeated CAN frames are legitimate. Clients never re-inject received
raw frames. A bridge acceptance does not prove physical delivery or receiver use.

TX permission expires after 3s without a ping; clients ping every 1s. Service has a 60s
hard limit and 512-frame ceiling, allowing only raw IDs for ISO/NMEA management and
route Group Functions. The application additionally enforces the precise existing
read-only route selector policy. The bridge is a raw-frame utility, not a PGN decoder.

At most 16 clients,128 pending TX frames per client,2s queue age,500 submitted frames/s
per client,64KiB output buffering. A global 5ms physical CAN write interval protects
small SocketCAN queues. EOF/cancel discards pending frames; already submitted kernel/
wire data cannot be recalled. No stale TX retries. CAN write/health faults revoke TX;
inspect the bus before restarting. CAN interface/error/drop state is checked every
0.5s, and error frames are also monitored. Slow/invalid clients are disconnected.

## Focused checks

Run `python3 tests/network_bridge_tests.py` from the repository. Windows application
fixtures use the `network_can_tests` CMake/CTest target. Physical tests require explicit opt-in
and private identity/key fixtures; they are not part of an ordinary test run.
The `network_can_peer` CMake helper target builds a disposable console instance of the same application
transport/core for different-computer qualification; its synthetic TX is restricted
to `lab` or interface names beginning `vcan`.
