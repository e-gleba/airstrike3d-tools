#pragma author divo-decrypt
#pragma description Divo Save File Parser
#pragma endian little

// No includes needed for basic types

struct Header {
    // [0x00:4] float32 1.0f
    // 1.0f in hex is 00 00 80 3F (Little Endian)
    float magic [[color("4A90E2"), name("Magic Number")]];

    // [0x04:260] 256-byte xor key
    u8 xor_key[256] [[color("50C878"), name("XOR Key")]];

    // [0x104:4] uint32 total
    // Split into CRC (low 16) and Padding (high 16)
    // format("hex") is a built-in display modifier
    u16 crc16 [[color("FFD700"), name("CRC16 Checksum"), format("hex")]];
    
    // "padding" is a keyword, so we must call this variable "reserved" or "pad"
    u16 reserved [[color("808080"), hidden]];
};

struct Payload {
    // [0x108] Encrypted bytes (Size: 0x574)
    u8 data[0x574] [[color("FF6B35"), name("Encrypted Blob")]];
};

struct DivoSaveFile {
    Header header [[name("File Header")]];
    Payload payload [[name("Game Data")]];
};

// Map the struct to the start of the file
DivoSaveFile save_file @ 0x00;