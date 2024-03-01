import time

from vita_socket import VitaSocket


HOST = '127.0.0.1'
PORT = 5002
MOD = 'tcp'


class VitaSocketWrapper:
    def __init__(self, host, port, buffer_size=2048, socket_mod='udp', little_endin=True):
        self.vita_socket = VitaSocket(buffer_size, little_endin)
        self.socket_mod = socket_mod
        self.host = host
        self.port = port


    def run_with_retry(self, run_fn):
        while True:
            try:
                run_fn(self.host, self.port)
                break
            except Exception as e:
                print(f"Error: {e}")
                time.sleep(1)


    def run(self):
        if self.socket_mod.lower() == 'udp':
            self.run_with_retry(self.vita_socket.run_udp)
        elif self.socket_mod.lower() == 'tcp':
            self.run_with_retry(self.vita_socket.run_tcp)
        else:
            raise ValueError(f"Invalid socket mod {self.socket_mod}")
        

    def getStreamIDs(self):
        return self.vita_socket.getStreamIDs()
    
    def getStream(self, stream_id):
        return self.vita_socket.getStream(stream_id)



def main():

    vita_socket = VitaSocketWrapper(HOST, PORT, socket_mod=MOD)
    vita_socket.run()

    while True:

        for stream_id in vita_socket.getStreamIDs():
            stream = vita_socket.getStream(stream_id)

            if stream.hasContextPacket():
                sample_rate = stream.getSampleRate()
                data = stream.getPacketData()
                print(f"Stream ID: {stream_id}, Sample Rate: {sample_rate}, Data: {len(data)}")

        time.sleep(0.1)


if __name__ == "__main__":
    main()