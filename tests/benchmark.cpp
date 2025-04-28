// mkdir -p build && cd build
// rm -rf * && cmake .. && make -j5
// ./benchmark 2000 500000 5000
// valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./benchmark 2000 500000 5000
// kcachegrind &

#include <iostream>
#include <chrono>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <string>

#include "../siamese.h"

int main(int argc, char *argv[])
{
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <window size> <packet count> <packet size>" << std::endl;
        return 1;
    }
    int window_size = atoi(argv[1]);
    int num_packets = atoi(argv[2]);
    int packet_size = atoi(argv[3]);

    siamese_init();
    SiameseEncoder encoder = siamese_encoder_create();
    if (!encoder)
    {
        std::cerr << "Error creating encoder" << std::endl;
        return 1;
    }

    const int fec_ratio = 6; // 1 in 6 = ~17%
    SiameseOriginalPacket packet;
    SiameseRecoveryPacket recovery;
    packet.DataBytes = packet_size;
    // recovery.DataBytes = packet_size + SIAMESE_MAX_ENCODE_OVERHEAD;

    auto data = std::vector<std::vector<uint8_t>>();
    for (int i = 0; i < 256; i++)
    {
        data.push_back(std::vector<uint8_t>(packet_size, i));
    }

    for (int i = 0; i < window_size; i++)
    {
        packet.PacketNum = i;
        packet.Data = data[i % 256].data();
        siamese_encoder_add(encoder, &packet);
    }
    std::cout << "Window loaded with " << window_size << " packets..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_packets; i++)
    {
        packet.PacketNum = i;
        packet.Data = data[i % 256].data();
        // siamese_encoder_add(encoder, &packet);

        if (i % fec_ratio == 0)
        {
            auto encode_result = siamese_encode(encoder, &recovery);

            if (encode_result != Siamese_NeedMoreData)
                continue;

            if (encode_result == Siamese_Success)
                if (i % (fec_ratio * 10) == 0)
                    siamese_encoder_remove_before(encoder, i);
            else
            {
                std::string error = "unknown";
                switch(encode_result){
                    case Siamese_InvalidInput:      error = "Siamese_InvalidInput";     break;
                    case Siamese_NeedMoreData:      error = "Siamese_NeedMoreData";     break;
                    case Siamese_MaxPacketsReached: error = "Siamese_MaxPacketsReached";break;
                    case Siamese_DuplicateData:     error = "Siamese_DuplicateData";    break;
                    case Siamese_Disabled:          error = "Siamese_Disabled";         break;
                    case SiameseResult_Count:       error = "SiameseResult_Count";      break;
                    case SiameseResult_Padding:     error = "SiameseResult_Padding";    break;
                }
                std::cerr << "Error " << error << " encoding after" << i << " pkts" << std::endl;
                return encode_result;
            }
        }
    }

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;

    std::cout << "Encoded: " << num_packets << " pkts" << std::endl;
    std::cout << "Encoded: " << num_packets / fec_ratio << " FEC" << std::endl;
    std::cout << "Time: " << elapsed.count() << " secs" << std::endl;
    double tp = num_packets / elapsed.count();
    std::cout << "Throughput: " << tp << " pkt/sec" << std::endl;
    tp =  (((double)num_packets * packet_size) / 125000) / elapsed.count();
    std::cout << "Throughput: " << tp << " Mbit/s" << std::endl;

    siamese_encoder_free(encoder);
    return 0;
}