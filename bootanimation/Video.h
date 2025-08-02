#ifndef VIDEO_H
#define VIDEO_H

#include <cstdint>
#include <cassert>
extern "C"
{
#include <x264.h>
#define LSMASH_INITIALIZE_CODEC_ID_HERE //codec fourccs should be defined
#include <lsmash.h>
#include <aacenc_lib.h>
#include <common/bytes.h>
#include <codecs/mp4a.h>
}
#include <string>

#define MP4SYS_ADTS_MAX_FRAME_LENGTH ( ( 1 << 13 ) - 1 )


class Video
{
private:
	//Encoder variables
	int nFrames;
	int nFramesWritten;
	int nFramesAfterWritten;
	uint8_t* pRGBBuffer;
	int32_t nWidth;
	int32_t nHeight;
	int32_t nFrameRate;
	size_t nSEISize;
	uint8_t* pSEIBuffer;
	uint32_t nMovieTs;
	uint32_t nVideoTs;
	uint32_t nVideoTrack;
	uint32_t nVideoSampleEntry;
	uint32_t nAudioTrack;
	uint32_t nAudioSampleEntry;
	uint64_t nTimeIncrement;
	int64_t nStartOffset;
	int64_t nFirstCTS;
	uint64_t nPrevCTS;
	int64_t nFirstDTS;
	int64_t nLastDTS;
	int64_t nPrevDTS;
	int64_t nLargestDTS1;
	int64_t nLargestDTS2;
	int64_t nLargestPTS1;
	int64_t nLargestPTS2;
	bool bVideoTrackCreated;
	bool bAudioTrackCreated;
	uint64_t nMediaTimescale;
	uint32_t nAudioSampleRate;
	uint32_t nAudioChannel;
	uint32_t nAudioSamples;
	int16_t* pAudioBuffer;
	uint64_t nAudioSamplesWritten;
	//X264
	x264_param_t stParam;
	x264_picture_t stPic;
	x264_t* pX264Handle;
	//L-SMASH
	lsmash_root_t* pRoot;
	lsmash_video_summary_t* pVideoSummary;
	lsmash_audio_summary_t* pAudioSummary;
	lsmash_file_parameters_t stFileParam;
	//FDK-AAC
	HANDLE_AACENCODER pAACHandle;
	AACENC_InfoStruct stAACInfo;

	bool WriteVideoHeaders(x264_nal_t* pNAL);
	bool WriteVideoFrame(uint8_t* pNALU, int nSize, x264_picture_t* pPicture);
	bool AddFrameInternal(x264_picture_t* pPicture);
public:
	Video(void);
	bool Open(std::string szFilename);
	void SetParams(uint32_t nDestWidth, uint32_t nDestHeight, uint32_t nDestFrameRate);
	void SetParamsAudio(uint32_t nDestSampleRate, uint32_t nSamples, uint32_t nDestChannel);
	bool Start(void);
	bool CreateVideoTrack(void);
	bool CreateAudioTrack(void);
	uint8_t* GetFrameData(void);
	bool AddFrame(void);
	bool AddAudio(int16_t* pAudioData, uint32_t nSamples);
	bool Close(void);
	~Video(void);
};

static uint8_t clampToU8(double v) {
	if (v < 0.0) v = 0;
	if (v > 1.0) v = 1.0;
	return (uint8_t)(v * 255.0);
}

#endif