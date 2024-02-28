#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <arpa/inet.h>
#include <unistd.h>
#include <ostream>
#include <string>
#include <cstring>
#include <atomic>
#include <time.h>

#include <vrt/vrt_read.h>
#include <vrt/vrt_string.h>
#include <vrt/vrt_types.h>
#include <vrt/vrt_util.h>



class VitaStream {
public:
    VitaStream(int id, size_t max_packets = 1000000) : stream_id(id), max_packets(max_packets) {
        packets.reserve(100000);
    }

    VitaStream(const VitaStream&) = delete;
    VitaStream& operator=(const VitaStream&) = delete;
    VitaStream(VitaStream&&) = delete;
    VitaStream& operator=(VitaStream&&) = delete;

    void addPacket(const vrt_packet& packet) {
        std::lock_guard<std::mutex> lock(stream_mutex);
        if (packet.header.packet_type == VRT_PT_IF_CONTEXT) {
            context_packet = packet;
        } else {
            packets.push_back(packet);
            auto* bytePtr = static_cast<uint8_t*>(packet.body); // Convert void* to uint8_t*
            std::vector<uint8_t> data = std::vector<uint8_t>(bytePtr, bytePtr + packet.words_body * 4);
            packet_data.push_back(data);

            if (packets.size() > max_packets) {
                // If we have more than max_packets then remove the oldest packet
                // We will remove half the packets
                packets.erase(packets.begin(), packets.begin() + packets.size() / 2);
                std::cout << "C++ Dropped packets, remaining:  " << packets.size() << std::endl;
                std::flush(std::cout);
            }
        }
    }

    std::vector<std::vector<uint8_t>> getPacketData() {
        std::lock_guard<std::mutex> lock(stream_mutex);
        //Copy packet data for return and clear the packet data
        // std::cout << "Packet size: " << packet_data.size() << std::endl;
        std::vector<std::vector<uint8_t>> data(packet_data);
        packet_data.clear();
        packets.clear();
        return data;
    }


    int getStreamID() const {
        return stream_id;
    }

    int getSampleRate() const {
        return 0 == context_packet.if_context.sample_rate ? 0 : context_packet.if_context.sample_rate;
    }

    int getPacketCount() const {
        return packets.size();
    }

    bool hasContextPacket() const {
        return context_packet.header.packet_type == VRT_PT_IF_CONTEXT;
    }

    double getFrequency() const {
        return context_packet.if_context.rf_reference_frequency_offset + context_packet.if_context.if_reference_frequency;
    }

private:
    std::vector<vrt_packet> packets;
    std::vector<std::vector<uint8_t>> packet_data;
    int stream_id;
    int max_packets;
    std::mutex stream_mutex;
    vrt_packet context_packet;
};


class VitaSocket {

    public:

        VitaSocket(int buffer_size, bool little_endian=true) : buffer_size(buffer_size), little_endian(true), running(true) {
            shared_buffer.reserve(buffer_size);
        }

        void stop_vita_socket() {
            running = false;
        }

        std::vector<int> getStreamIDs() {
            std::lock_guard<std::mutex> lock(stream_id_mutex);
            std::vector<int> ids;
            for (const auto& stream : streams) {
                ids.push_back(stream.first);
            }
            return ids;
        }

        VitaStream* getStream(int stream_id) {
            std::lock_guard<std::mutex> lock(stream_id_mutex);
            if (streams.find(stream_id) == streams.end()) {
                return nullptr;
            }
            return &streams.at(stream_id);
        }


        void join() {
            receiverThread.join();
            parserThread.join();
        }

        int run_tcp(const char* host, int port) {

            struct sockaddr_in servaddr;
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                throw std::runtime_error("socket creation failed");
            }

            int enable = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
            }

            std::memset(&servaddr, 0, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(port);

            if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
                throw std::runtime_error("Invalid address/ Address not supported");
            }

            // Connect to the server
            if (connect(sockfd, reinterpret_cast<const struct sockaddr *>(&servaddr), sizeof(servaddr)) < 0) {
                throw std::runtime_error("Connection Failed");
            }

            receiverThread = std::thread(&VitaSocket::receiveData, this, sockfd, servaddr);
            parserThread = std::thread(&VitaSocket::parseData, this);

            return 0;
        }


        int run_udp(const char* host, int port){

            struct sockaddr_in servaddr;
            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd < 0) {
                throw std::runtime_error("socket creation failed");
            }

            int enable = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
            }

            std::memset(&servaddr, 0, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(port);

            if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
                throw std::runtime_error("Invalid address/ Address not supported");
            }

            if (bind(sockfd, reinterpret_cast<const struct sockaddr *>(&servaddr), sizeof(servaddr)) < 0) {
                throw std::runtime_error("bind failed");
            }

            receiverThread = std::thread(&VitaSocket::receiveData, this, sockfd, servaddr);
            parserThread = std::thread(&VitaSocket::parseData, this);

            return 0;
        }



    private:

        int buffer_size;

        bool little_endian;

        std::mutex buffer_mutex;
        
        std::mutex stream_id_mutex;

        // std::vector<uint32_t> shared_buffer;
        std::vector<uint8_t> shared_buffer;

        std::atomic<bool> running;

        std::thread receiverThread;
        std::thread parserThread;
        
        std::map<int, VitaStream> streams;

        void addPacketToStream(int stream_id, const vrt_packet& packet) {
            std::lock_guard<std::mutex> lock(stream_id_mutex);
            if (streams.find(stream_id) == streams.end()) {
                // Doing a no copy emplace
                streams.emplace(std::piecewise_construct, 
                std::forward_as_tuple(stream_id), 
                std::forward_as_tuple(stream_id));


            }
            streams.at(stream_id).addPacket(packet);
        }

        uint32_t littleEndianToUint32(const std::vector<uint8_t>& vec, size_t index) {
            return static_cast<uint32_t>(vec[index]) << 24|
                static_cast<uint32_t>(vec[index + 1]) << 16 |
                static_cast<uint32_t>(vec[index + 2]) << 8 |
                static_cast<uint32_t>(vec[index + 3]);
        }

        int processVRT(std::vector<uint8_t> &local_buffer, int local_buffer_offset) {

            // Only use the valid words e.g 4 bytes
            int local_buffer_size = local_buffer.size() - local_buffer_offset;
            local_buffer_size = local_buffer_size - (local_buffer_size % 4);

            
            std::vector<uint32_t> uint32_vector; 
            uint32_vector.reserve(local_buffer_size / 4);

            if (little_endian){
                // Need to cast local_buffer to unit32_t 
                // Have to be carefull because the buffer is little endian, but we are on a big endian system
                for (int i = local_buffer_offset; i <= local_buffer_size-4; i += 4) {
                    uint32_vector.push_back(littleEndianToUint32(local_buffer, i));
                }
            } else {
                // We are on a big endian system so we can just cast the buffer to uint32_t
                uint32_vector = std::vector<uint32_t>(reinterpret_cast<uint32_t*>(local_buffer.data() + local_buffer_offset), reinterpret_cast<uint32_t*>(local_buffer.data() + local_buffer_size));
            }
            

            int size = uint32_vector.size();

            int32_t offset = 0;

            while (offset + 40 < size) {
                // std::cout << "Main loop" << std::endl;
                struct vrt_packet p;
                // std::cout<< "Offset: " << offset << " Size: " << size << std::endl;
                int32_t rv = vrt_read_packet(uint32_vector.data() + offset, uint32_vector.size(), &p, true);
                if (rv == -1){
                    break;
                    // if (size - offset < 1000){
                    //     // Could be buffer is to small or may just be a bad packet
                    //     break;
                    // }
                    // // Shift the buffer by 1 byte and try again
                    // return (offset * 4) + 1;
                }
                if (rv < 0) {
                    std::cerr << "Failed to parse packet: " << vrt_string_error(rv) << " " << rv << std::endl;
                    // Shift the buffer by 1 byte and try again
                    return (offset * 4) + 1;
                }

                // Check if the packet is within the buffer
                // VRT dose not allways insure that the packet is complete
                if (rv + offset > size){
                    std::cout << "Packet size: " << rv << std::endl;
                    // std::cout << "Offset: " << offset << " Size: " << size << std::endl;
                    return (offset * 4) + 1;
                }


                // Check if the packet.body (void*) is pointing within the buffer
                // If not then the packet failed to parse
                // Nullptr is allowed due to context packets
                if (p.header.packet_type <= VRT_PT_EXT_DATA_WITH_STREAM_ID){
                    // cast p.body to unit32_t*  
                    uint32_t* body = static_cast<uint32_t*>(p.body);
                    if (body < uint32_vector.data() + offset || body + p.words_body > uint32_vector.data() + size){
                        std::cout << "Packet size: " << rv << std::endl;
                        std::cout << "Offset: " << offset << " Size: " << size << std::endl;
                        return (offset * 4) + 1;
                    }

                    if (p.words_body <= 0){
                        std::cerr << "Packet body size is not valid: " << p.words_body << std::endl; 
                        return (offset * 4) + 1;
                    }
                }

                // Check to make sure that packet.header.packet_count top 4 bits are 0
                // Only the last 4 bits are used
                int top_4_bits = p.header.packet_count & 0xF0;
                if (top_4_bits != 0){
                    std::cerr << "Packet Count top 4 bits are not 0 they are " << top_4_bits << std::endl;
                    return (offset * 4) + 1;
                }

                if (p.header.packet_type == VRT_PT_IF_CONTEXT) {
                    // Check if the packet is a context packet that is has a positive sample rate
                    if (p.if_context.sample_rate <= 0) {
                        std::cerr << "Sample rate is not valid" << std::endl;
                        return (offset * 4) + 1;
                    }

                    if (p.body != nullptr){
                        std::cerr << "Context packet body is not null" << std::endl;
                        return (offset * 4) + 1;
                    }
                }

                addPacketToStream(p.fields.stream_id, p);
                offset += rv;
            }

            if (offset > size){
                std::cout << "Offset: " << offset << " Size: " << size << std::endl;
                throw std::runtime_error("Offset is larger than size");
            }
            // std::cout << "Offset: " << offset << " Size: " << size << std::endl;
            return (offset * 4);
        }

        void print_info() {
            // Rather then printing line by line lets build the data and then print it
            std::string data;
            data += "C++: Vita Socket INFO shared buffer size " + std::to_string(shared_buffer.size()) + "\n";
            data += "C++: Stream Count: " + std::to_string(streams.size()) + "\n";
            for (const auto& stream : streams) {
                if (stream.second.getSampleRate() > 0){
                    data += "   C++: Stream ID: " + std::to_string(stream.first) + " Sample Rate: " + std::to_string(stream.second.getSampleRate()) + " Packet Count: " + std::to_string(stream.second.getPacketCount()) + "\n";
                }
            }
            std::cout << data << std::flush;
        }

        void parseData() {
            std::vector<uint8_t> local_buffer;
            local_buffer.reserve(buffer_size);
            int local_buffer_offset = 0;
            clock_t last_print = clock();


        

            while (running) {

                if (clock() - last_print > CLOCKS_PER_SEC*5) {
                    last_print = clock();
                    print_info();
                }

                {
                    std::unique_lock<std::mutex> lock(buffer_mutex);
                    if (shared_buffer.size() == 0) {
                        lock.unlock();
                        // Sleep for 1ms
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }
                    // Add shared buffer to the end of the local buffer
                    local_buffer.resize(shared_buffer.size() + local_buffer.size());
                    std::copy(shared_buffer.begin(), shared_buffer.end(), local_buffer.end() - shared_buffer.size());
                    shared_buffer.clear();
                    lock.unlock();
                }

                int offset = processVRT(local_buffer, local_buffer_offset);
                offset += local_buffer_offset;

                if (offset > 0 && offset < local_buffer.size()) {
                    int remaining = (local_buffer.size() - offset);

                    
                    if (remaining > 2000){
                        // Only do copy if we have aligned to the packet boundary
                        // Otherwise we will have to shift large amounts of data
                        // One byte at a time
                        local_buffer_offset = offset;
                    } else {
                        std::copy(local_buffer.end() - remaining, local_buffer.end(), local_buffer.begin());
                        local_buffer.resize(remaining);
                        local_buffer_offset = 0;
                    }

                } else if (offset == -1) {
                    std::cerr << "Clearing buffer due to failure to parse packet" << std::endl;
                    local_buffer.clear();
                } else if (offset == local_buffer.size()) {
                    std::cout << "Clearing local_buffer buffer " << offset << " " << local_buffer.size() << std::endl;
                    local_buffer.clear();
                }
                else if (offset == 0 && local_buffer.size() > 160){
                    std::cout << "This should not happen " << offset << " " << local_buffer.size() << std::endl;
                } else if (offset == 0){
                    std::cout << "No data to process" << std::endl;
                } else {
                    std::cerr << "Unknown error  Offset: " << offset << " Size: " << local_buffer.size() << std::endl;
                    local_buffer.clear();
                }
            }
        }

        void receiveData(int sockfd, struct sockaddr_in servaddr) {
            uint8_t buffer[buffer_size];
            socklen_t len = sizeof(servaddr);

            // int offset = 0;

            while (running) {
                int n = recv(sockfd, buffer, buffer_size, 0);
                if (n < 0) {
                    perror("recvfrom failed");
                    exit(EXIT_FAILURE);
                }

                std::unique_lock<std::mutex> lock(buffer_mutex);
                shared_buffer.resize(shared_buffer.size() + n); // Resize the vector
                std::memcpy(shared_buffer.data() + shared_buffer.size() - n, buffer, n);
                lock.unlock();
            }
            close(sockfd);
        }
};

int main() {
    // run_tcp("127.0.0.1", 5002);
    // run_udp("127.0.0.1", 5002);

    VitaSocket vita_socket(2048);

    // vita_socket.run_tcp("127.0.0.1", 5002);
    vita_socket.run_udp("127.0.0.1", 5002);


    // Get the data every second
    while (true){

        //get stream ids
        std::vector<int> ids = vita_socket.getStreamIDs();
        std::cout << "Stream IDs: " << std::endl;
        std::cout << std::flush;
        for (const auto& id : ids) {
            std::cout << id << " " << std::endl;
            // std::vector<std::vector<uint8_t>> data = getStream(id)->getPacketData();
            VitaStream* stream = vita_socket.getStream(id);
            if (stream == nullptr) {
                continue;
            }
            int num_packets = stream->getPacketCount();
            std::cout << "Stream ID: " << id << " Packet Count: " << num_packets << std::endl;
            std::vector<std::vector<uint8_t>> data = stream->getPacketData();
            std::cout << "Data size: " << data.size() << std::endl;
            // Flush the print buffer
            std::cout << std::flush;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    vita_socket.join();
    return 0;
}
