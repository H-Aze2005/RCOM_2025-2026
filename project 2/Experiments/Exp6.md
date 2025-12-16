# Experiment 6 - TCP connections

## Step 1
Compile your donwload application in tuxY3

## Step 2
In tuxY3, restart capturing with Wireshark and run your application

## Step 3
Verify if file has arrived correctly, stop capturing and save the log

## Step 4
Using Wireshark, observe packets exchanged including:
- TCP control and data connections, and its phases (establishment, data, termination)
- Data transferred through the FTP control connection
- TCP ARQ mechanism
- TCP congestion control mechanism in action
- NOTE: use also the Wireshark Statistics tools (menu) to study the TCP phases, ARQ and congestion control mechanism

## Step 5
Repeat the download in tuxY3 but now, in the middle of the transfer, start a new download in tuxY2

## Questions

- How many TCP connections are opened by your FTP application?
- In what connection is transported the FTP control information?
- What are the phases of a TCP connection?
- How does the ARQ TCP mechanism work? What are the relevant TCP fields? What relevant information can be observed in the logs?
- How does the TCP congestion control mechanism work? What are the relevant fields. How did the throughput of the data connection evolve along the time? Is it
according to the TCP congestion control mechanism?
- Is the throughput of a TCP data connections disturbed by the appearance of a second TCP connection? How?