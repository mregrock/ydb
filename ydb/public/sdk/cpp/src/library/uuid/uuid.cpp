#include "uuid.h"

#include <util/stream/str.h>

namespace NYdb {
namespace NUuid {

static void WriteHexDigit(ui8 digit, IOutputStream& out) {
    if (digit <= 9) {
        out << char('0' + digit);
    }
    else {
        out << char('a' + digit - 10);
    }
}

static void WriteHex(ui16 bytes, IOutputStream& out, bool reverseBytes = false) {
    if (reverseBytes) {
        WriteHexDigit((bytes >> 4) & 0x0f, out);
        WriteHexDigit(bytes & 0x0f, out);
        WriteHexDigit((bytes >> 12) & 0x0f, out);
        WriteHexDigit((bytes >> 8) & 0x0f, out);
    } else {
        WriteHexDigit((bytes >> 12) & 0x0f, out);
        WriteHexDigit((bytes >> 8) & 0x0f, out);
        WriteHexDigit((bytes >> 4) & 0x0f, out);
        WriteHexDigit(bytes & 0x0f, out);
    }
}

void UuidToString(ui16 dw[8], IOutputStream& out) {
    WriteHex(dw[1], out);
    WriteHex(dw[0], out);
    out << '-';
    WriteHex(dw[2], out);
    out << '-';
    WriteHex(dw[3], out);
    out << '-';
    WriteHex(dw[4], out, true);
    out << '-';
    WriteHex(dw[5], out, true);
    WriteHex(dw[6], out, true);
    WriteHex(dw[7], out, true);
}

std::string UuidBytesToString(const std::string& in) {
    TStringStream ss;
    
    UuidBytesToString(TString(in), ss);

    return std::string(ss.Str());
}

void UuidBytesToString(const std::string& in, IOutputStream& out) {
    ui16 dw[8];
    std::memcpy(dw, in.data(), sizeof(dw));
    NUuid::UuidToString(dw, out);
}

void UuidHalfsToString(ui64 low, ui64 hi, IOutputStream& out) {
    union {
        ui16 dw[8];
        ui64 half[2];
    } buf;
    buf.half[0] = low;
    buf.half[1] = hi;
    NUuid::UuidToString(buf.dw, out);
}

void UuidHalfsToByteString(ui64 low, ui64 hi, IOutputStream& out) {
    union {
        char bytes[16];
        ui64 half[2];
    } buf;
    buf.half[0] = low;
    buf.half[1] = hi;
    out.Write(buf.bytes, 16);
}

}
}

