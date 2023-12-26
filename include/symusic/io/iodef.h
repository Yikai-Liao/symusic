//
// Created by lyk on 2023/12/24.
//

#ifndef LIBSYMUSIC_IO_IODEF_H
#define LIBSYMUSIC_IO_IODEF_H

namespace symusic {

// define a enum for data formats
enum class DataFormat {
    MIDI,       // MIDI,            https://en.wikipedia.org/wiki/MIDI
    MusicXML,   // MusicXML,        https://en.wikipedia.org/wiki/MusicXML
    ABC,        // ABC Notation,    https://en.wikipedia.org/wiki/ABC_notation
    ZPP,        // zpp_bits, c++20, customised binary format, https://github.com/eyalz800/zpp_bits
    ALPACA,     // alpaca,   c++17, customised binary format, https://github.com/p-ranav/alpaca
    CEREAL,     // cereal,   c++11, customised binary format, https://github.com/USCiLab/cereal
};

}

#endif //LIBSYMUSIC_IO_IODEF_H
