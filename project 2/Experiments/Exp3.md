# Experiment 3 - Configure a Router in Linux

## Step 1
Connect cables between machines and the switch where Y is the bench number
- tuxY4_E2 -> 8

**On tuxY3 run**
```bash
sudo ifconfig if_e2 172.16.Y0.253/24
```

**Add it to bridgeY1 on E2**
```bash
/interface bridge port remove [find interface=ether8]

/interface bridge port add bridge=bridgeY1 interface=ether8
```

**Enable IP forwarding**
```bash
sudo sysctl net.ipv4.ip_forward=1
```

**Disable ICMP echo-ignore-broadcast**
```bash
sudo sysctl net.ipv4.icmp_echo_ignore_broadcasts=0
```

## Step 2
Observe MAC addresses and IP addresses in tuxY4.if_el and tuxY4.if_e2

-> INSERT IMAGE HERE <-

## Step 3
Reconfigure tuxY3 and tuxY2 so that each of them reach the other
**On tuxY3**
```bash
sudo route add -net 172.16.Y1.0/24 gw 172.16.Y0.254
```

**On tuxY2**
```bash
sudo route add -net 172.16.Y0.0/24 gw 172.16.Y1.253
```

## Step 4
Observe the routes available at the 3 tuxes (**route -n**)

**On tuxY2**
-> INSERT IMAGE HERE <-

**On tuxY3**
-> INSERT IMAGE HERE <-

**On tuxY4**
-> INSERT IMAGE HERE <-

## Step 5 - 6 - 7
Start capture in tuxY3

From tuxY3 ping the other network interfaces (172.16.Y0.254, 172.16.Y1.253, 172.16.Y1.1) and verify if there is connectivity

Stop the capture and save the logs

[Wireshark capture log](logs/exp3_passos_5_6_7.pcapng)

## Step 8


## Step 9


## Step 10


## Step 11