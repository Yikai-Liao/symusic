//
// Created by lyk on 23-12-16.
//
#include "symusic/mtype.h"
#include "symusic/utils.h"

namespace symusic::details {
std::string strip_non_utf_8(const std::string & str) {
    const size_t f_size = str.size();
    std::string to;
    to.reserve(f_size);
    for (size_t i = 0; i < f_size; i++) {
        //control char
        const u8 c = static_cast<u8>(str[i]);
        if (c < 32) {
            //allow only \t \n \r
            if (c == 9 || c == 10 || c == 13) {
                to.append(1, static_cast<char>(c));
            } continue;
        }
        if (c < 127) {  //normal ASCII
            to.append(1, static_cast<char>(c));
            continue;
        }
        //control char (nothing should be defined here either ASCII, ISO_8859-1 or UTF8, so skipping)
        if (c < 160) {
            //fix microsoft mess, add euro
            if (c == 128) {
                to.append(1, -30);  // 226 for unsigned
                to.append(1, -126); // 130 for unsigned
                to.append(1, -84);  // 172 for unsigned
            }
            //fix IBM mess, add NEL = \n\r
            if (c == 133) {
                to.append(1, 10);
                to.append(1, 13);
            } continue;
        }
        //invalid for UTF8, converting ASCII
        if (c < 192) {
            to.append(1, static_cast<char>(194));
            to.append(1, static_cast<char>(c));
            continue;
        }
        //invalid for UTF8, converting ASCII
        if (c < 194) {
            to.append(1, static_cast<char>(195));
            to.append(1, static_cast<char>(c - 64));
            continue;
        }
        //possibly 2byte UTF8
        if (c < 224 && i + 1 < f_size) {
            //valid 2byte UTF8
            if (const u8 c2 = static_cast<u8>(str[i + 1]); c2 > 127 && c2 < 192) {
                //control char, skipping
                if (!(c == 194 && c2 < 160)) {
                    to.append(1, c);
                    to.append(1, c2);
                } i++; continue;
            }
        }
        //possibly 3byte UTF8
        else if (c < 240 && i + 2 < f_size) {
            const u8 c2 = static_cast<u8> (str[i + 1]);
            const u8 c3 = static_cast<u8> (str[i + 2]);
            //valid 3byte UTF8
            if (c2 > 127 && c2 < 192 && c3 > 127 && c3 < 192) {
                to.append(1, static_cast<char>(c));
                to.append(1, static_cast<char>(c2));
                to.append(1, static_cast<char>(c3));
                i += 2; continue;
            }
        }
        //possibly 4byte UTF8
        else if (c < 245 && i + 3 < f_size) {

            const u8 c2 = static_cast<u8> (str[i+1]);
            const u8 c3 = static_cast<u8> (str[i + 2]);
            const u8 c4 = static_cast<u8> (str[i + 3]);
            //valid 4byte UTF8
            if (c2 > 127 && c2 < 192 && c3 > 127 && c3 < 192 && c4 > 127 && c4 < 192) {
                to.append(1, static_cast<char>(c));
                to.append(1, static_cast<char>(c2));
                to.append(1, static_cast<char>(c3));
                to.append(1, static_cast<char>(c4));
                i += 3; continue;
            }
        }
        //invalid UTF8, converting ASCII (c>245 || string too short for multibyte)
        to.append(1, static_cast<char>(195));
        to.append(1, static_cast<char>(c - 64));
    } return to;
}
}
