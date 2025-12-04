# Experiment 1 - Configure an IP Network

## Step 1
Connect cables between machines and the switch where Y is the bench number
- tuxY3_E1 -> 2

- tuxY4_E1 -> 4

## Step 2
**On tuxY3 run**
```bash
sudo ifconfig if_e1 172.16.Y0.1/24
```

**On tuxY4 run**
```bash
sudo ifconfig if_e1 172.16.Y0.254/24
```

## Step 3
**IP and MAC address for tuxY3**

-> INSERT IMAGE HERE <-

**IP and MAC address for tuxY4**

-> INSERT IMAGE HERE <-

## Step 4
**On tuxY4 use the ping command to verify connectivity**
```bash
sudo ping 172.16.10.1
```

## Step 5
Inspect forwarding (**route -n**) and ARP (**arp -a**) tables

-> INSERT IMAGE HERE <-

## Step 6
Delete ARP table entries in tuxY3 (**arp -d upaddress**)
```bash
sudo arp -d 172.16.Y0.254
```

## Step 7 - 8 - 9
Start Wireshark in tuxY3.if_e1 and start capturing packets

In tuxY3, ping tuxY4 for a few seconds

Stop capturing packets

## Step 10
Save the log

[Wireshark capture log](logs/exp1_passos_7_8_9.pcapng)
