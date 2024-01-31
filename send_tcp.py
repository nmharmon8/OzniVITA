
import socket
import time

with open("/md1/jibber_jabber_data/jibber_jabber_test_data/rooftop/dev_100MHz_MB_92pt5_488kHz.vrt", "rb") as f:
    data = f.read()


socket_addr = ('localhost', 5002)


with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    s.bind(socket_addr)

    s.listen()

    while True:

        conn, addr = s.accept()

        send_chunk_size = 123
        sleep = 1.0 / (len(data) / send_chunk_size)

        while True:

            for i in range(0, len(data), send_chunk_size):
                # s.sendto(data[i:i + send_chunk_size], socket_addr)
                conn.sendall(data[i:i + send_chunk_size])
                time.sleep(sleep)

        
           