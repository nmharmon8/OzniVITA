
import socket
import time

with open("/md1/jibber_jabber_data/jibber_jabber_test_data/rooftop/dev_100MHz_MB_92pt5_488kHz.vrt", "rb") as f:
    data = f.read()


socket_addr = ('localhost', 5002)


with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    while True:

        send_chunk_size = 128 * 6
        sleep = 17.0 / (len(data) / send_chunk_size)
        # sleep /= 10

        while True:
            for i in range(0, len(data), send_chunk_size):
                s.sendto(data[i:i + send_chunk_size], socket_addr)
                # time.sleep(0.0000000001)
                time.sleep(sleep)

            # time.sleep(0.5)

        
           