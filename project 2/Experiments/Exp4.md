# Experiment 4 - Configure a Commercial Router and Implement NAT

## Step 1

Rc_e2 -> ether24

Connect ether1 of Rc to the lab network on the PY.24 (**with NAT enabled by default**) and the ether2 of Rc to a port on bridgeY1.

```bash
/interface bridge port remove [find interface=ether24]

/interface bridge port add bridge=bridgeY1 interface=ether24
```

Configure the IP addresses of RC through the router serial console
```bash
/ip address add address=172.16.1.Y1/24 interface=ether1

/ip address add address=172.16.Y1.254/24 interface=ether2
```

## Step 2
Verify routes and add if necessary:
- in tuxY3 routes for 172.16.Y1.0/24 and 172.16.1.0/24
- in tuxY4 route for 172.16.1.0/24
- in tuxY2 routes for 172.16.Y0.0/24 and 172.16.1.0/24
- in Rc add route for 172.16.Y0.0/24

gateways are added on the opposite side to the router

```bash
sudo route add -net <destination address> gw <gateway address>
```

On the GtkTerm connected to the router

```bash
/ip route add dst-address=172.16.Y0.0/24 gateway=172.16.Y1.253
```

## Step 3
Using ping commands and Wireshark, verify if tuxY3 can ping all the network interfaces of tuxY2, tuxY4 and Rc

**On tuxY3**

Start Wireshark capture and ping:

- ping 172.16.Y0.254
- ping 172.16.Y1.1
- ping 172.16.Y1.254

[Wireshark capture log](logs/exp4_passos_3.pcapng)

## Step 4
no tuxY2:  
* echo 0 \> /proc/sys/net/ipv4/conf/eth1/accept\_redirects  
* echo 0 \> /proc/sys/net/ipv4/conf/all/accept\_redirects  
* route del –net 172.16.Y0.0 gw 172.16.Y1.253 netmask 255.255.255.0  
* route add \-net 172.16.Y0.0/24 gw 172.16.Y1.254   
* ping 172.16.Y0.1  
* **começar captura de wireshark dentro do tuxY2**  
* traceroute \-n 172.16.Y0.1  
* **acabar captura de wireshark dentro do tuxY2**  
* route del –net 172.16.Y0.0 gw 172.16.Y1.254 netmask 255.255.255.0	  
* route add \-net 172.16.Y0.0/24 gw 172.16.Y1.253  
* **começar captura de wireshark dentro do tuxY2**  
* traceroute \-n 172.16.Y0.1  
* **acabar captura de wireshark dentro do tuxY2**  
* **começar captura de wireshark dentro do tuxY2**  
* echo 1 \> /proc/sys/net/ipv4/conf/eth1/accept\_redirects  
* echo 1 \> /proc/sys/net/ipv4/conf/all/accept\_redirects  
* traceroute \-n 172.16.Y0.1  
* **acabar captura de wireshark dentro do tuxY2**  

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