#pragma once
struct Codec {
    int volume = 37;
    unsigned volume_set_calls = 0;
    void SetOutputVolume(int value) {
        volume = value;
        ++volume_set_calls;
    }
    int output_volume() const { return volume; }
    int output_sample_rate() { return 24000; }
};
struct Board {
    static Board& GetInstance() {
        static Board board;
        return board;
    }
    Codec* GetAudioCodec() { return &codec; }
    Codec codec;
};
