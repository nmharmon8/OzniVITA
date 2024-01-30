import sys
import signal
import time

import numpy as np
from matplotlib import pyplot as plt

# sys.path.append('/Airborne/jj/jibberjabber/vrt/build')
import vita_socket_py

def signal_handler(sig, frame):
    vita_socket_py.stop_vita_socket()
    exit()

signal.signal(signal.SIGINT, signal_handler)
vita_socket_py.run_udp("127.0.0.1", 5002)
# vita_socket_py.run_tcp("127.0.0.1", 5002)

complex16_dtype = np.dtype([("real", '<i2'), ("imag", '<i2')])

i = 0
while True:
    stream_ids = vita_socket_py.getStreamIDs()
    print("Stream IDs:", stream_ids)
    
    for stream_id in stream_ids:

        stream = vita_socket_py.getStream(stream_id)

        if stream.hasContextPacket():
            

            sample_rate = vita_socket_py.getStream(stream_id).getSampleRate()

            data = vita_socket_py.getStream(stream_id).getPacketData()
            signals = []
            for packet_data in data:
                packet_data = bytes(packet_data)
                packet_data = np.frombuffer(packet_data, dtype=complex16_dtype)
                packet_data = packet_data['real'] + 1j*packet_data['imag']
                signals.append(packet_data)
            if len(signals) == 0:
                continue
            signals = np.concatenate(signals)
            print(f"Data {len(signals) / sample_rate} seconds of data")
            # fig, ax = plt.subplots()
            # ax.specgram(signals, Fs=sample_rate)
            # ax.set_xlabel("Time (s)")
            # ax.set_ylabel("Frequency (Hz)")
            # ax.set_title(f"Stream ID {stream_id}")
            # plt.savefig(f"stream_id_{stream_id}_{i}.png")
            # plt.close()
            i += 1
            


    time.sleep(1)



    # g++ vita_socket.cpp -lvrt -lpthread -o vita_socket
    # cd build/; make; cd ../; python test.py