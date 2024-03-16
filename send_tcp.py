
import socket
import time
from tqdm import tqdm

with open("/md1/jibber_jabber_data/jibber_jabber_test_data/rooftop/dev_100MHz_MB_92pt5_488kHz.vrt", "rb") as f:
    data = f.read()


socket_addr = ('localhost', 5003)


with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    s.bind(socket_addr)

    s.listen()

    conn, addr = s.accept()

    while True:

        
        # conn.sendall(data)
        # time.sleep(2)
        # continue

        send_chunk_size = 123
        sleep = 1.0 / (len(data) / send_chunk_size)

        while True:

            for i in tqdm(range(0, len(data), send_chunk_size)):
                # s.sendto(data[i:i + send_chunk_size], socket_addr)
                conn.sendall(data[i:i + send_chunk_size])
                time.sleep(sleep)

        
           