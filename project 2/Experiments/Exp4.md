# Experiment 4 - Configure a Commercial Router and Implement NAT

## Step 1
Connect ether1 of Rc to the lab network on the PY.24 (**with NAT enabled by default**) and the ether2 of Rc to a port on bridgeY1.

```bash
/interface bridge port remove [find interface=ether24]

/interface bridge port add bridge=bridgeY1 interface=ether24
```

Configure the IP addresses of RC through the router serial console
```bash
/ip address add address=172.16.1.Y0/24 interface=ether1

/ip address add address=172.16.1.Y1.254/24 interface=ether2
```

## Step 2
Verify routes and add if necessary:
- in tuxY3 routes for 172.16.Y1.0/24 and 172.16.1.0/24
- in tuxY4 route for 172.16.1.0/24
- in tuxY2 routes for 172.16.Y0.0/24 and 172.16.1.0/24
- in Rc add route for 172.16.Y0.0/24

```bash
sudo route add -net <destination address> gw <gateway address>
```

## Step 3
Using ping commands and Wireshark, verify if tuxY3 can ping all the network interfaces of tuxY2, tuxY4 and Rc

## Step 4


## Step 5


## Step 6


## Step 7


## Questions

- How to configure a static route in a commercial router?
- What are the paths followed by the packets, with and without ICMP redirect
enabled, in the experiments carried out and why?
- How to configure NAT in a commercial router?
- What does NAT do?
- What happens when tuxY3 pings the FTP server with the NAT disabled? Why?