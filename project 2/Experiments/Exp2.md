# Experiment 2 - Implement two bridges in a switch

## Step 1
Connect cables between machines and the switch where Y is the bench number

- tuxY2_E1 -> 5

**On tuxY2 run**
```bash
sudo ifconfig if_e1 172.16.Y1.1/24
```

**IP and MAC address for tuxY3**

-> INSERT IMAGE HERE <-

## Step 2
Create two **bridges** in the switch: bridgeY0 and bridgeY1

```bash
/interface bridge add name=bridgeY0
/interface bridge add name=bridgeY1
```

## Step 3
Remove the ports where tuxY3, tuxY4 and tuxY2 are connected from the default bridge (**bridge**) and add them to the corresponding ports to bridgeY0 and bridgeY1

```bash
/interface bridge port remove [find interface=ether2]
/interface bridge port remove [find interface=ether4]
/interface bridge port remove [find interface=ether5]

/interface bridge port add bridge=bridgeY0 interface=ether2
/interface bridge port add bridge=bridgeY0 interface=ether4
/interface bridge port add bridge=bridgeY1 interface=ether5
```

## Step 4 - 5 - 6
Start the capture at tuxY3.if_e1

In tuxY3, ping tuxY4 and then ping tuxY2

Stop the capture and save the log

[Wireshark capture log](logs/exp2_passos_4_5_6.pcapng)

## Step 7 - 8 - 9
Start new captures in tuxY2.if_e1, tuxY3.if_e1, tuxY4.if_e1

In tuxY3, do ping broadcast (**ping -b 172.16.Y0.255**) for a few seconds

Stop the captures and save the logs

[Wireshark capture log tuxY2](logs/exp2_passos_7_8_9_tuxY2.pcapng)

[Wireshark capture log tuxY3](logs/exp2_passos_7_8_9_tuxY3.pcapng)

[Wireshark capture log tuxY4](logs/exp2_passos_7_8_9_tuxY4.pcapng)

## Step 10
Repeat steps 7 - 8 - 9 but now do 
```bash
sudo ping -b 172.16.Y1.255
```

[Wireshark capture log tuxY2](logs/exp2_passo_10_tuxY2.pcapng)

[Wireshark capture log tuxY3](logs/exp2_passo_10_tuxY3.pcapng)

[Wireshark capture log tuxY4](logs/exp2_passo_10_tuxY4.pcapng)

## Questions

- How to configure bridgeY0?
- How many broadcast domains are there? How can you conclude it from the logs?