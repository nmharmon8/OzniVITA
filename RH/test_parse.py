from rh_file_parser import VRTParser

parser = VRTParser()
# vrt_file = '/code/jibberjabber/scripts/rooftop2/dev_100MHz_MB_92pt5_488kHz.vrt'
# vrt_file = '/code/jibberjabber/scripts/rooftop/0x00003113_0x00010e27_90.866MHz_2025-02-20T18_54_29.351Z_shortened.vrt'
vrt_file = '/code/jibberjabber/scripts/rooftop/0x000030e7_0x80000000_123.003MHz_2025-03-25T20_57_42.602Z_AM.vrt'
if parser.parseVRTFile(vrt_file):
    # Access the data
    iq_data = parser.getIQData()  # Returns numpy array of complex64
    stream_id = parser.getStreamID()
    center_freq = parser.getCenterFrequency()
    sample_rate = parser.getSampleRate()
    ctx_packets = parser.getContextPacketCount()
    data_packets = parser.getDataPacketCount()
    
    print(f"Parsed {ctx_packets} context packets and {data_packets} data packets")
    print(f"Center Frequency: {center_freq} Hz")
    print(f"Sample Rate: {sample_rate} Hz")
    print(f"IQ Samples: {len(iq_data)}")
    print(f"IQ length in seconds: {len(iq_data) / sample_rate}")
    print(f"Stream ID: {stream_id}")
    #Print first 10 iq samples
    print(iq_data[:10])
else:
    print("Failed to parse VRT file")