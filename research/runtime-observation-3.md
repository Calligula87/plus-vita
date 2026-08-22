# Runtime observation 3: header-only UDP traffic

Observation date: 2026-08-22

Scope: one user-authorized successful game session using Windows Packet Monitor
with an IPv4/UDP filter on network interfaces. Each recorded frame was truncated
to 42 bytes, the size of ordinary Ethernet, IPv4, and UDP headers. The raw ETL
and derived text remain in the ignored private-capture directory.

No application payload, credentials, cookies, tokens, session identifiers,
keys, or authenticated messages were collected.

## Aggregate traffic

At the Wi-Fi interface, the capture recorded:

| Direction | Packets | Frame bytes | Average frame size |
| --- | ---: | ---: | ---: |
| Received | 203,691 | 169,427,777 | 831.8 bytes |
| Transmitted | 14,322 | 1,839,632 | 128.4 bytes |
| Total | 218,013 | 171,267,409 | n/a |

These totals include all IPv4 UDP visible at the interface during the test and
cannot be attributed exclusively to PlayStation Plus. However, the sustained,
strongly download-dominant burst coincided with the user-reported game session
and dominates the observation.

Traffic exceeded 100 KB per second from approximately `14:45:40` through
`14:50:02` capture-local time. The busiest observed second carried about 3.01
MB in 3,724 frames. Per-minute totals during the main interval were:

| Minute | Packets | Frame bytes |
| --- | ---: | ---: |
| `14:45` | 9,410 | 8,151,909 |
| `14:46` | 37,247 | 28,351,117 |
| `14:47` | 37,705 | 28,514,664 |
| `14:48` | 47,807 | 39,096,510 |
| `14:49` | 79,951 | 62,459,269 |
| `14:50` | 5,847 | 4,682,788 |

The most common transmitted frame size was 101 bytes. Received frames were
distributed across many sizes, including frequent frames around 808–819 bytes
and 1,479 bytes. This direction and size asymmetry is consistent with a media
stream plus smaller upstream control/input traffic, but metadata alone does not
establish message roles.

## Endpoint limitation

The 42-byte truncation boundary is sufficient for ordinary Ethernet frames.
This computer's Wi-Fi packet representation includes a larger 802.11 header and
LLC encapsulation. Consequently, the retained bytes ended inside the IP header
for most Wi-Fi frames, before the source/destination tuple could be decoded.

The few decodable tuples belonged to local broadcast and multicast discovery
traffic and are unrelated to the game stream. No stream-server IP address can
be responsibly reported from this run.

This limitation also confirms that the privacy boundary did not expose UDP
application data: on the relevant Wi-Fi frames it did not reach the complete
transport header.

## Current conclusion

Together with observation 2, this run strengthens the evidence for a
download-heavy UDP media path owned by the native player during a successful
session. It provides initial packet-rate, direction, volume, and frame-size
characteristics without retaining payloads.

The next collector revision should capture at an IP-layer Packet Monitor
component rather than the Wi-Fi link layer. That would make a fixed
IPv4-plus-UDP header limit sufficient to decode endpoints while still ending
before application payload. No broader or longer packet capture is warranted.

## Follow-up component-selection result

A follow-up test selected both the active Wi-Fi adapter's primary component ID
and the inventory's `TCP/IPv4 - L3/L4` secondary ID. On this Windows build,
Packet Monitor resolved both selectors to the parent Ethernet-edge component.
Reducing retention to 28 bytes therefore truncated inside the Ethernet/IP
headers and did not expose application payload or a decodable stream tuple.

Further Packet Monitor component-selection tests are not warranted. The next
approach should use TCP/IP ETW endpoint and bind events with packet capture
explicitly disabled.
