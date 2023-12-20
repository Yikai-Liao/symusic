#include "Score.hpp"
#include "cassert"
using namespace score;

int main(int argc, char *argv[]) {
    if(argc == 2) {
        std::string filename = std::string(argv[1]);
        std::cout << "Filename: " << filename << std::endl;
        try {
            auto s = Score<Tick>::from_file(filename);
            std::cout << "Score notes: " << s.note_num() << std::endl;
            auto [data, in, out] = zpp::bits::data_in_out();
            const std::string c = "Tick";
            out(c, s);
            std::cout << "Size of Score Bin [zpp_bits]: " << static_cast<double>(data.size()) / 1024 << " KB" <<std::endl;
            std::string a;
            Score<Tick> s2;
            in(a, s2).or_throw();
            std::cout << "Score [zpp_bits] notes: " << s2.note_num() << std::endl;
            // show data in hex
            std::cout << "Data in hex: " << std::endl;
            for (const auto& byte : data) {
                std::cout << std::hex << static_cast<int>(byte) << " ";
            }   std::cout << std::dec << std::endl;
        } catch(const char* e) {
            std::cout << e << std::endl;
            exit(EXIT_FAILURE);
        };
    } else {
        std::cout << "Usage: ./note_count <midi_file_name>" << std::endl;
    }
    return 0;
}
