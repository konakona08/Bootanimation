/*
**----------------------------------------------------------------------------
**  Includes
**----------------------------------------------------------------------------
*/

#define STB_VORBIS_HEADER_ONLY
#include <extras/stb_vorbis.c> /* Enables Vorbis decoding. */
#define MINIAUDIO_IMPLEMENTATION
#include "Audio.h"

/*
**----------------------------------------------------------------------------
**  Definitions
**----------------------------------------------------------------------------
*/

#define AUDIO_ASSERT(expr, msg, ret) \
	{ \
		if (!(expr)) \
		{ \
			fprintf(stderr, "%s\n", msg); \
			return ret; \
		} \
	}

#define AUDIO_ASSERT_NORET(expr, msg) \
	{ \
		if (!(expr)) \
			fprintf(stderr, "%s\n", msg); \
	}

/*
**----------------------------------------------------------------------------
**  Type Definitions
**----------------------------------------------------------------------------
*/

/*
**---------------------------------------------------------------------------
**  Global variables
**---------------------------------------------------------------------------
*/

/*
**---------------------------------------------------------------------------
**  Internal variables
**---------------------------------------------------------------------------
*/

/*
**---------------------------------------------------------------------------
**  Function(internal use only) Declarations
**---------------------------------------------------------------------------
*/

Audio::Audio(void)
{
	pAudioBuffer = nullptr;
	nSampleRate = 0;
	nChannels = 0;
	llFrameCount = 0;
}

bool Audio::Open(std::string szFilename)
{
	stConfig = ma_decoder_config_init(ma_format_s16, 0, 0);

	AUDIO_ASSERT(ma_decoder_init_file(szFilename.c_str(), &stConfig, &stDecoder) == MA_SUCCESS, "fail open audio", false);
	AUDIO_ASSERT(ma_data_source_get_length_in_pcm_frames(&stDecoder, &llFrameCount) == MA_SUCCESS, "Failed to get length", false);

	nChannels = stDecoder.outputChannels;	
	nSampleRate = stDecoder.outputSampleRate;

    return true;
}

void Audio::ExtractInfo(int* pnSampleRate, uint64_t *pnSamples, int* pnChannels)
{
	AUDIO_ASSERT(pnSampleRate != nullptr && pnChannels != nullptr && pnSamples != NULL, "Invalid pointers", );
	*pnSampleRate = nSampleRate;
	*pnChannels = nChannels;
	*pnSamples = llFrameCount;
}

bool Audio::Convert(void)
{
	ma_uint64 frameRead = 0;
	pAudioBuffer = (int16_t*)malloc((llFrameCount * OUT_SAMPLE_RATE / stDecoder.outputSampleRate) * nChannels * sizeof(int16_t) + (256 * 1024));//add 256k as boundary
	memset(pAudioBuffer, 0, (llFrameCount * OUT_SAMPLE_RATE / stDecoder.outputSampleRate) * nChannels * sizeof(int16_t) + (256 * 1024));
	AUDIO_ASSERT(ma_decoder_read_pcm_frames(&stDecoder, pAudioBuffer, llFrameCount, &frameRead) == MA_SUCCESS, "Failed to read PCM frames", false);
	return true;
}

int16_t* Audio::GetAudioBuffer(void)
{
	AUDIO_ASSERT(pAudioBuffer != nullptr, "Audio buffer is null", NULL);
	return pAudioBuffer;
}

bool Audio::Close(void)
{
	ma_decoder_uninit(&stDecoder);
	return true;
}

void Audio::GetAudioDuration(ma_uint32* pnMilliseconds)
{
	AUDIO_ASSERT_NORET(pnMilliseconds != nullptr, "Invalid pointers");
	*pnMilliseconds = static_cast<ma_uint32>(llFrameCount * 1000 / OUT_SAMPLE_RATE);
}

Audio::~Audio(void)
{
	Close();
	if (pAudioBuffer != nullptr) {
		free(pAudioBuffer);
		pAudioBuffer = nullptr;
	}
}