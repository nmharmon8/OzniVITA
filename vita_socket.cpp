#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <atomic>
#include <time.h>

#include <vrt/vrt_read.h>
#include <vrt/vrt_string.h>
#include <vrt/vrt_types.h>
#include <vrt/vrt_util.h>


#define BUFFER_SIZE 1024

std::mutex buffer_mutex;
std::mutex stream_mutex;
std::mutex stream_id_mutex;

std::vector<uint32_t> shared_buffer;

std::atomic<bool> data_available(false);
std::atomic<bool> running(true);

void stop_vita_socket() {
    running = false;
}

class VitaStreams {
public:
    VitaStreams(int id) : stream_id(id) {
        packets.reserve(100000);
    }

    void addPacket(const vrt_packet& packet) {
        std::lock_guard<std::mutex> lock(stream_mutex);
        if (packet.header.packet_type == VRT_PT_IF_CONTEXT) {
            context_packet = packet;
        } else {
            packets.push_back(packet);
            auto* bytePtr = static_cast<uint8_t*>(packet.body); // Convert void* to uint8_t*
            std::vector<uint8_t> data = std::vector<uint8_t>(bytePtr, bytePtr + packet.words_body * 4);
            packet_data.push_back(data);
        }
    }

    std::vector<std::vector<uint8_t>> getPacketData() {
        std::lock_guard<std::mutex> lock(stream_mutex);
        //Copy packet data for return and clear the packet data
        std::cout << "Packet size: " << packet_data.size() << std::endl;
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

private:
    std::vector<vrt_packet> packets;
    std::vector<std::vector<uint8_t>> packet_data;
    int stream_id;
    vrt_packet context_packet;
};

std::map<int, VitaStreams> streams;

void addPacketToStream(int stream_id, const vrt_packet& packet) {
    std::lock_guard<std::mutex> lock(stream_id_mutex);
    if (streams.find(stream_id) == streams.end()) {
        streams.emplace(stream_id, VitaStreams(stream_id));
    }
    streams.at(stream_id).addPacket(packet);
}

std::vector<int> getStreamIDs() {
    std::lock_guard<std::mutex> lock(stream_id_mutex);
    std::vector<int> ids;
    for (const auto& stream : streams) {
        ids.push_back(stream.first);
    }
    return ids;
}

VitaStreams* getStream(int stream_id) {
    std::lock_guard<std::mutex> lock(stream_id_mutex);
    if (streams.find(stream_id) == streams.end()) {
        return nullptr;
    }
    return &streams.at(stream_id);
}

int processVRT(uint32_t *b, int size) {

    int32_t offset = 0;

    while (offset + 40 < size) {
        struct vrt_packet p;
        int32_t rv = vrt_read_packet(b + offset, size, &p, true);
        if (rv == -1){
            //Buffer too small
            break;
        }
        if (rv < 0) {
            std::cerr << "Failed to parse packet: " << vrt_string_error(rv) << " " << rv << std::endl;
            // Shift the buffer by 1 byte and try again
            return offset + 1;
        }

        // Check if the packet is within the buffer
        // VRT dose not allways insure that the packet is complete
        if (rv + offset > size){
            std::cout << "Packet size: " << rv << std::endl;
            std::cout << "Offset: " << offset << " Size: " << size << std::endl;
            // throw std::runtime_error("Total is too large");
            return offset + 1;
        }


        // Check if the packet.body (void*) is pointing within the buffer
        // If not then the packet failed to parse
        // Nullptr is allowed due to context packets
        if (p.header.packet_type <= VRT_PT_EXT_DATA_WITH_STREAM_ID){
            // cast p.body to unit32_t*  
            uint32_t* body = static_cast<uint32_t*>(p.body);
            if (body < b + offset || body + p.words_body > b + size){
                std::cout << "Packet size: " << rv << std::endl;
                std::cout << "Offset: " << offset << " Size: " << size << std::endl;
                return offset + 1;
            }

            if (p.words_body <= 0){
                std::cerr << "Packet body size is not valid: " << p.words_body << std::endl; 
                return offset + 1;
            }
        }


        // Check to make sure that packet.header.packet_count top 4 bits are 0
        // Only the last 4 bits are used
        int top_4_bits = p.header.packet_count & 0xF0;
        if (top_4_bits != 0){
            std::cerr << "Packet Count top 4 bits are not 0 they are " << top_4_bits << std::endl;
            return offset + 1;
        }


       
        if (p.header.packet_type == VRT_PT_IF_CONTEXT) {
            // Check if the packet is a context packet that is has a positive sample rate
            if (p.if_context.sample_rate <= 0) {
                std::cerr << "Sample rate is not valid" << std::endl;
                return offset + 1;
            }

            if (p.body != nullptr){
                std::cerr << "Context packet body is not null" << std::endl;
                return offset + 1;
            }
        }


        // Get packet count from p.header.packet_count only the last 4 bits are used
        // Mask the last 4 bits and convert to int
        // int packet_count = p.header.packet_count & 0x0F;
        // std::cout << "Packet count: " << packet_count << std::endl;

        addPacketToStream(p.fields.stream_id, p);
        offset += rv;
    }

    // Remaining data processing
    // int remaining = (size - offset);

    if (offset > size){
        std::cout << "Offset: " << offset << " Size: " << size << std::endl;
        throw std::runtime_error("Offset is larger than size");
    }
    return offset;
}

void print_info() {
    std::cout << "Vita Socket INFO:" << std::endl;
    std::cout << "Stream Count: " << streams.size() << std::endl;
    for (const auto& stream : streams) {
        if (stream.second.getSampleRate() > 0){
            std::cout << "Stream ID: " << stream.first << " Sample Rate: " << stream.second.getSampleRate() << " Packet Count: " << stream.second.getPacketCount() << std::endl;
        }
    }
}

void parseData() {
    std::vector<uint32_t> local_buffer;
    local_buffer.reserve(BUFFER_SIZE);

    clock_t last_print = clock();


    int last_shared_buffer_size = 0;

    while (running) {

        if (clock() - last_print > CLOCKS_PER_SEC) {
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

        int offset = processVRT(local_buffer.data(), local_buffer.size());
        if (offset > 0 && offset < local_buffer.size()) {
            int remaining = (local_buffer.size() - offset);
            // std::cout << "Remaining: " << remaining << " Offset: " << offset << " Size: " << local_buffer.size() << std::endl;
            // std::cout << "Last value: " << local_buffer[local_buffer.size() - 1] << std::endl;
            std::copy(local_buffer.end() - remaining, local_buffer.end(), local_buffer.begin());
            local_buffer.resize(remaining);
            // std::cout << "Buffer size: " << local_buffer.size() << std::endl;
            // std::cout << "Last value: " << local_buffer[local_buffer.size() - 1] << std::endl;
        } else if (offset == -1) {
            // std::cerr << "Clearing buffer due to failure to parse packet" << std::endl;
            local_buffer.clear();
        } else if (offset == local_buffer.size()) {
            // std::cout << "Clearing local_buffer buffer " << offset << " " << local_buffer.size() << std::endl;
            local_buffer.clear();
        }
        else if (offset == 0 && local_buffer.size() > 40){
            std::cout << "This should not happen" << std::endl;
        } else if (offset == 0){
            // std::cout << "No data to process" << std::endl;
        } else {
            std::cerr << "Unknown error  Offset: " << offset << " Size: " << local_buffer.size() << std::endl;
            local_buffer.clear();
        }
    }
}

void receiveData(int sockfd, struct sockaddr_in servaddr) {
    uint8_t buffer[BUFFER_SIZE];
    socklen_t len = sizeof(servaddr);

    while (running) {
        int n = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (n < 0) {
            perror("recvfrom failed");
            exit(EXIT_FAILURE);
        }

        if (n % 4 != 0) {
            std::cerr << "Received data not divisible by 4" << std::endl;
            throw std::runtime_error("Received data not divisible by 4");
        }

        // Endian conversion
        for (int i = 0; i < n; i += 4) {
            std::swap(buffer[i], buffer[i + 3]);
            std::swap(buffer[i + 1], buffer[i + 2]);
        }

        std::unique_lock<std::mutex> lock(buffer_mutex);
        size_t num_uint32_elements = n / sizeof(uint32_t);

        shared_buffer.resize(shared_buffer.size() + num_uint32_elements); // Resize the vector
        std::memcpy(shared_buffer.data() + shared_buffer.size() - num_uint32_elements, buffer, n);
        data_available = true;
        lock.unlock();
    }
    close(sockfd);
}



std::thread receiverThread;
std::thread parserThread;

void run_tcp(const char* host, int port) {

    struct sockaddr_in servaddr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(host);
    servaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sockfd, reinterpret_cast<const struct sockaddr *>(&servaddr), sizeof(servaddr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }

    receiverThread = std::thread(receiveData, sockfd, servaddr);
    parserThread = std::thread(parseData);
}


void run_udp(const char* host, int port){

    struct sockaddr_in servaddr;
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(host);
    servaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, reinterpret_cast<const struct sockaddr *>(&servaddr), sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    receiverThread = std::thread(receiveData, sockfd, servaddr);
    parserThread = std::thread(parseData);

}

int main() {
    // run_tcp("127.0.0.1", 5002);
    run_udp("127.0.0.1", 5002);


    // Get the data every second
    while (true){

        //get stream ids
        std::vector<int> ids = getStreamIDs();
        std::cout << "Stream IDs: " << std::endl;
        std::cout << std::flush;
        for (const auto& id : ids) {
            std::cout << id << " " << std::endl;
            // std::vector<std::vector<uint8_t>> data = getStream(id)->getPacketData();
            VitaStreams* stream = getStream(id);
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

    receiverThread.join();
    parserThread.join();
    return 0;
}
