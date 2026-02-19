#ifndef AUDIO_H
#define AUDIO_H

#define OUT_SAMPLE_RATE 44100

#include <cstdint>
#include <cassert>
extern "C"
{
#include <miniaudio.h>
}
#include <string>

class Audio
{
private:
	ma_decoder_config stConfig;
	ma_decoder stDecoder;
	int16_t* pAudioBuffer;
	int nSampleRate;
	int nChannels;
	ma_uint64 llFrameCount;
	ma_uint32 nAudioSeconds;
	ma_uint32 nAudioMilliseconds;
	
public:
	Audio(void);
	bool Open(std::string szFilename);
	bool OpenMemory(unsigned char* pData, size_t dataSize);
	void ExtractInfo(int* pnSampleRate, uint64_t* pnSamples, int* pnChannels);
	bool Convert(void);
	int16_t* GetAudioBuffer(void);
	void GetAudioDuration(ma_uint32* pnMilliseconds);
	bool Close(void);
	~Audio(void);
};

#endif