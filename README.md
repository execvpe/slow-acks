# Slow ACKs

Delay outgoing packets / ACKs to artificially slow down your internet speed (IPv4 only).

This software exploits the mechanism of congestion control in web servers,
which reduces the data flow when [ACK](https://en.wikipedia.org/wiki/Acknowledgement_(data_networks))
packets from the client arrive slowly.

```
+--------------+       +------+       +-------------------+       +--------+
|    Client    | ----> | PC + | ----> |      Gateway      | ----> |  Web-  |
| 172.28.12.34 |  ACK  | slow |  ACK  | cc:ce:ab:cd:ef:ff | <---- | Server |
+--------------+       +------+       +-------------------+       +--------+
        ^                                       |
        |           Packet from Server          |
        +---------------------------------------+
```

## Disclaimer

### This program is still a work in progress!

Undefined behavior such as race conditions or other fatal errors can occur,
including those that can destroy important data or compromise the security of your system.
Therefore, I recommend **using this software in a test environment under constant observation** only.

Features may be deleted or changed in functionality without further notice.
It may be that the documentation below is not updated in time and therefore
may not document the real behavior of the program.

Please let me know or open a Pull Request with a suitable correction if you found any bugs in this project.
Any help is appreciated. :)

### Use this software at your own risk!

## Build

`make all cap` (requires `gcc` and `setcap`)

## Usage

This software receives IPv4 packets from a given IP address (the client you want to throttle) and,
after delaying it by the set delay, forwards them to a gateway MAC address (most likely your router).

When the packet buffer is full, the newest packets are silently discarded without notification to the client.

```shell
./slow --ifnam=<interface name> --ip=<forward ip> --mac=<gateway mac> --bbuf=<packet buffer size> --delay=<delay in ms>
```

Use a command like `ip link` to get the name of the network interface you want to monitor (e.g. `eno1`, `wlp0s20f3`).
The command `ip neigh` will show you the MAC address of your router.

**Experimental features:**

Send `SIGUSR1` to reset the delay to 0 ms.
Send `SIGUSR2` to increase the forwarding delay by 5 ms.

**Don't forget to statically configure the client you want to throttle**,
i.e. set your computer's IPv4 address as the gateway IP.

## Example

```shell
$ ./slow --ifnam=eno1 --ip=172.28.12.34 --mac=cc:ce:ab:cd:ef:ff --bbuf=1024 --delay=100
Interface Name: eno1
Gateway MAC: cc:ce:ab:cd:ef:ff
Forwarding IP: 172.28.12.34
Forwarding Delay: 100 ms
Packet Buffer Size: 1024 Packets/Frames

Debug Packet Count: 0
Eth Frame Size (bytes): 74
IPv4 Packet Size (bytes): 60
Source Address: 172.28.12.34
Destination Address: 12.34.56.78
Protocol: 0x06 (TCP)
Identification: 53735
Src MAC: ab:cd:ef:ab:cd:ef
Dst MAC: aa:bb:cc:dd:ee:ff

...

^C[slow] Interrupted
[slow] Sockets closed
```
