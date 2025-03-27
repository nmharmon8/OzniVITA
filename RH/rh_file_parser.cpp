#include <iostream>
#include <fstream>
#include <vector>
#include <complex>

#include "BasicControlPacket.h"
#include "BasicDataPacket.h"

#define MSGBUFSIZE 16384

class VRTParser {
private:
    uint32_t streamID;
    double centerFreq_hz;
    double sampleRate_sps;
    std::vector<std::complex<float>> iqData;
    uint32_t contextPacketCount;
    uint32_t dataPacketCount;
    bool debugOutput;  // New debug flag

public:
    VRTParser() : streamID(0), centerFreq_hz(0.0), sampleRate_sps(0.0), 
                  contextPacketCount(0), dataPacketCount(0), debugOutput(false) {}  // Initialize debug flag

    void setDebugOutput(bool debug) { debugOutput = debug; }  // New setter for debug flag

    bool parseVRTFile(const std::string& filePath) {
        // Clear any existing data
        iqData.clear();
        contextPacketCount = 0;
        dataPacketCount = 0;
        
        // Open the binary file
        std::ifstream inputFile(filePath, std::ios::binary);

        // Check if the file opened successfully
        if (!inputFile) {
            std::cerr << "Error opening file." << std::endl;
            return false;
        }

        // Determine the size of the file
        inputFile.seekg(0, std::ios::end);
        std::streamsize fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        // Create a buffer to hold the contents of the file
        std::vector<char> buffer(fileSize);

        // Read the file into the buffer
        vrt::BasicVRTPacket vrt;
        if (!inputFile.read(buffer.data(), fileSize)) {
            std::cerr << "Error reading file." << std::endl;
            return false;
        }

        // Process the binary data
        if (debugOutput) {
            std::cout << "Read " << fileSize << " bytes:\n";
            for (std::size_t i = 0; i < std::min<std::size_t>(10, buffer.size()); ++i) {
                std::cout << std::hex << static_cast<int>(static_cast<unsigned char>(buffer[i])) << ' ';
            }
            std::cout << std::dec << std::endl;
        }

        void *ptr = buffer.data();
        iqData.reserve(fileSize / 8); // Conservative estimate: 8 bytes per complex sample
        
        while (ptr < (buffer.data() + fileSize)) {
            // read header
            vrt = BasicVRTPacket(ptr, 4);
            uint32_t packet_len = vrt.getPacketLength();

            if (packet_len == 0) {
                break;
            }

            // if context
            if (vrt.isContext()) {
                BasicContextPacket ctx = vrt::BasicContextPacket(ptr, packet_len);
                if (debugOutput) {
                    std::cout << ctx.toString() << std::endl;
                }
                
                // Store context packet information
                streamID = ctx.getStreamIdentifier();
                
                // Check if frequency information is available
                if (ctx.getFrequencyRF()) {
                    centerFreq_hz = ctx.getFrequencyRF();
                    //print center freq offset                        
                }
                
                // Check if sample rate information is available
                if (ctx.getSampleRate()) {
                    sampleRate_sps = ctx.getSampleRate();
                }
                
                contextPacketCount++;
            } else {
                // Data packet processing
                if (debugOutput) {
                    std::cout << "Data Packet" << std::endl;
                }
                BasicDataPacket data = vrt::BasicDataPacket(ptr, packet_len);
                if (debugOutput) {
                    std::cout << data.toString() << std::endl;
                    std::cout << "Payload length: " << data.getPayloadLength() << std::endl;
                }
                
                // Extract IQ samples from the data packet
                uint32_t pl_len = data.getPayloadLength();
                
                // Try to get the payload format from the packet
                vrt::PayloadFormat pf = data.getPayloadFormat();
                
                // Calculate number of complex samples (each complex sample is 2 int16 values)
                size_t numComplexSamples = pl_len / 4; // 4 bytes per complex sample (2 bytes I + 2 bytes Q)
                std::vector<std::complex<float>> tempSamples(numComplexSamples);
                
                if (!pf.isNullValue()) {
                    if (debugOutput) {
                        std::cout << "Data Type: " << pf.getDataType() << ", Complex: " << (pf.isComplex() ? "Yes" : "No") << std::endl;
                    }
                    
                    // Create a temporary buffer for int16 data
                    std::vector<int16_t> rawData(pl_len / 2); // 2 bytes per int16
                    
                    // Extract the raw data
                    data.getData(pf, rawData.data(), true);
                    
                    // Convert int16 pairs to complex<float>
                    for (size_t i = 0; i < numComplexSamples; i++) {
                        float real = rawData[i*2] / 32768.0f;     // Normalize by 2^15
                        float imag = rawData[i*2+1] / 32768.0f;
                        tempSamples[i] = std::complex<float>(real, imag);
                    }
                } else {
                    if (debugOutput) {
                        std::cout << "Warning: Null payload format, assuming int16 complex" << std::endl;
                    }
                    
                    // Set up payload format for int16 complex data
                    pf.setDataType(vrt::DataType_Int16);
                    pf.setProcessingEfficient(true);
                    pf.setRealComplexType(vrt::RealComplexType_ComplexCartesian);
                    
                    // Create a temporary buffer for int16 data
                    std::vector<int16_t> rawData(pl_len / 2);
                    
                    // Extract the raw data
                    data.getData(pf, rawData.data(), true);
                    
                    // Convert int16 pairs to complex<float>
                    for (size_t i = 0; i < numComplexSamples; i++) {
                        float real = rawData[i*2] / 32768.0f;     // Normalize by 2^15
                        float imag = rawData[i*2+1] / 32768.0f;
                        tempSamples[i] = std::complex<float>(real, imag);
                    }
                }
                
                // Append to our main IQ data vector
                iqData.insert(iqData.end(), tempSamples.begin(), tempSamples.end());
                dataPacketCount++;
            }
            ptr += packet_len;
        }
        //print first 10 iq samples, uncomment to test python matches
        // std::cout << "First 10 IQ samples:" << std::endl;
        // for (size_t i = 0; i < std::min(size_t(10), iqData.size()); i++) {
        //     std::cout << iqData[i].real() << " + " << iqData[i].imag() << "i" << std::endl;
        // }
        return (contextPacketCount > 0 && dataPacketCount > 0);
    }

    // Getters
    uint32_t getStreamID() const { return streamID; }
    double getCenterFrequency() const { return centerFreq_hz; }
    double getSampleRate() const { return sampleRate_sps; }
    const std::vector<std::complex<float>>& getIQData() const { return iqData; }
    
    // New getters for packet counts
    uint32_t getContextPacketCount() const { return contextPacketCount; }
    uint32_t getDataPacketCount() const { return dataPacketCount; }
};

// Optional: Keep the main function for standalone testing
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }
    
    VRTParser parser;
    parser.setDebugOutput(true);  // Enable debug output for main()
    bool success = parser.parseVRTFile(argv[1]);
    
    if (success) {
        // Print summary
        std::cout << "# of context packets: " << parser.getContextPacketCount() << std::endl;
        std::cout << "# of data packets: " << parser.getDataPacketCount() << std::endl;
        std::cout << "# of packets: " << parser.getContextPacketCount() + parser.getDataPacketCount() << std::endl;
        std::cout << "Stream ID: " << parser.getStreamID() << std::endl;
        std::cout << "Center Frequency: " << parser.getCenterFrequency() << " Hz" << std::endl;
        std::cout << "Sample Rate: " << parser.getSampleRate() << " Hz" << std::endl;
        std::cout << "Total IQ samples collected: " << parser.getIQData().size() << std::endl;

        // Example of using the IQ data
        std::cout << "\nFirst 5 IQ samples (if available):" << std::endl;
        for (size_t i = 0; i < std::min(size_t(5), parser.getIQData().size()); i++) {
            std::cout << "Sample " << i << ": " << parser.getIQData()[i].real() 
                     << " + " << parser.getIQData()[i].imag() << "i" << std::endl;
        }
    } else {
        std::cerr << "Failed to parse the VRT file." << std::endl;
        return 1;
    }
    
    return 0;
}
