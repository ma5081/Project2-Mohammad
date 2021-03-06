# Project-2 Mohammad


## Implementing Go-Back-N


* Implemented Go-Back-N by increasing the window size and sending that number of packets before checking for ACKs


## Implementing Reliability and Moving Windows


* Implemented by saving the last ACK to last ACK + Window Size as the possible-to-send packets.
* Implementing a last sent as a cursor for currently unsent packets to allow the separation of resent packets and newly sent ones
* Receiver gets a handshake at the beginning from the sender to let it know the amount of packets to be expected.
    * Using this, it creates a dynamic boolean array to keep track of buffered data.
    * It buffers the data now if out of order
    * Can look through the whole list of packets to check if anything has been missed *(mostly for the latest window size, but also as a safety net)*


## Implementing Congestion Control


* Added SlowStart, increasing the window with every ACK up to the ssthresh
* Decreasing ssthresh when a timeout happens to start fast retransmit *(in rdt_senderV.c, the fast recovery is not applied, so the dupe ACKs go into SlowStart)*
* Applied Fast Recovery to rdt_sender.c to start the AIMD instantly without decreasing the ssthresh (at the ssthresh that would have been implemented if SlowStart was to happen)


## Implementing Graph


* Appended the **CWND.csv** file whenever the window was about to change and after it changes
* On the receiver, appended a CSV file with the (time, packet size, packet received) whenever a packet was received into **TPT.csv**
* using the **plot.py**, use
    * **\<DIR>** is the name of the Directory, use . or .. for current and the parent Directory respectively
    * **\<TRACE>** is the name of the trace file
    * implemented the offset in the trace file, there is a constant in the **line 70 of plot.py** code that seems arbitrary and might need adjusting for the first run (but it seems like it works afterwards for the next ones once it is set the first time)
    * if the time taken to send the file is bigger than the trace file loop, the value will keep looping over the previously set values
```
python3 plot.py -d <DIR> -n TPT.csv -tr <TRACE>
```

* using the **plotW.py**, use
    * **\<DIR>** is the name of the Directory, use . or .. for current and the parent Directory respectively
```
python3 plotW.py -d <DIR> -n CWND.csv
```


## Using the code


* The code can be used in the same way the starter code works, the inputs and outputs are the same bar the addition of the CSV files to the outputs
