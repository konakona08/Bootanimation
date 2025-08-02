/*
**----------------------------------------------------------------------------
**  Includes
**----------------------------------------------------------------------------
*/

#include "Video.h"

/*
**----------------------------------------------------------------------------
**  Definitions
**----------------------------------------------------------------------------
*/

#define VIDEO_ASSERT(expr, msg, ret) \
	{ \
		if (!(expr)) \
		{ \
			fprintf(stderr, "%s\n", msg); \
			return ret; \
		} \
	}

#define VIDEO_ASSERT_NORET(expr, msg) \
	{ \
		if (!(expr)) \
			fprintf(stderr, "%s\n", msg); \
	}

#define H264_NALU_LENGTH_SIZE 4

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

Video::Video(void)
{
	nFrames = nFramesWritten = 0;
	nFirstDTS = nLastDTS = nLargestDTS1 = nLargestDTS2 = 0;
	pRGBBuffer = NULL;
	nWidth = nHeight = 0;
	nFrameRate = 0;
	nSEISize = 0;
	pSEIBuffer = NULL;
	pRoot = NULL;
	pVideoSummary = NULL;
	pX264Handle = NULL;
	bVideoTrackCreated = false;
    nFrames = 0;
    nFramesAfterWritten = 0;
    nAudioSamplesWritten = 0;
}

bool Video::Open(std::string szFilename)
{
	pRoot = lsmash_create_root();
	VIDEO_ASSERT(pRoot, "no pRoot created.....", false);
	VIDEO_ASSERT(lsmash_open_file(szFilename.c_str(), 0, &stFileParam) >= 0, "can't open video file", false);
	pVideoSummary = (lsmash_video_summary_t*)lsmash_create_summary(LSMASH_SUMMARY_TYPE_VIDEO);
	VIDEO_ASSERT(pVideoSummary, "video summary not created", false);
	pVideoSummary->sample_type = ISOM_CODEC_TYPE_AVC1_VIDEO;
    return true;
}

bool Video::Start(void)
{
    x264_param_default_preset(&stParam, "ultrafast", NULL);
    stParam.i_bitdepth = 8;
    stParam.i_csp = X264_CSP_I420;
    stParam.i_width = nWidth;
    stParam.i_height = nHeight;
    stParam.b_vfr_input = 1;
    stParam.i_fps_num = nFrameRate;
    stParam.i_fps_den = 1;
    stParam.i_timebase_num = stParam.i_fps_den;
    stParam.i_timebase_den = stParam.i_fps_num;
    stParam.rc.i_rc_method = X264_RC_CRF;
    stParam.rc.f_rf_constant = 24;

    x264_param_apply_fastfirstpass(&stParam);

    VIDEO_ASSERT(x264_param_apply_profile(&stParam, "main") == 0, "failed apply main profile", false);

    stParam.b_annexb = 0;
    stParam.b_repeat_headers = 0;

    pX264Handle = x264_encoder_open(&stParam);
    VIDEO_ASSERT(pX264Handle != NULL, "encoder open fail", false);

    x264_encoder_parameters(pX264Handle, &stParam);

    VIDEO_ASSERT(x264_picture_alloc(&stPic, stParam.i_csp, stParam.i_width, stParam.i_height) == 0, "failed alloc picture", false);

    //fix that might do

    nMediaTimescale = (uint64_t)stParam.i_timebase_den;
    nTimeIncrement = (uint64_t)stParam.i_timebase_num;

    lsmash_brand_type stBrands[6];
    memset(&stBrands, 0, sizeof(stBrands));
    uint32_t nBrandCount = 0;
    stBrands[nBrandCount++] = ISOM_BRAND_TYPE_MP42;
    stBrands[nBrandCount++] = ISOM_BRAND_TYPE_MP41;
    stBrands[nBrandCount++] = ISOM_BRAND_TYPE_ISOM;

    stFileParam.major_brand = stBrands[0];
    stFileParam.brands = stBrands;
    stFileParam.brand_count = nBrandCount;
    stFileParam.minor_version = 0;
    VIDEO_ASSERT(lsmash_set_file(pRoot, &stFileParam) != NULL, "failed to add output file", false);

    lsmash_movie_parameters_t movie_param;
    lsmash_initialize_movie_parameters(&movie_param);
    VIDEO_ASSERT(lsmash_set_movie_parameters(pRoot, &movie_param) == 0, "failed to set movie parameters", false);

    nMovieTs = lsmash_get_movie_timescale(pRoot);
    VIDEO_ASSERT(nMovieTs, "movie timescale", false);

    return true;
}

bool Video::CreateVideoTrack(void)
{
	if (bVideoTrackCreated == false)
	{
        nVideoTrack = lsmash_create_track(pRoot, ISOM_MEDIA_HANDLER_TYPE_VIDEO_TRACK);
        VIDEO_ASSERT(nVideoTrack != 0, "failed to create a video track", false);

        pVideoSummary->width = stParam.i_width;
        pVideoSummary->height = stParam.i_height;
        uint32_t nDispWdt = stParam.i_width << 16;
        uint32_t nDispHgt = stParam.i_height << 16;

        if (stParam.vui.i_sar_width && stParam.vui.i_sar_height) {
            double sar = (double)stParam.vui.i_sar_width / stParam.vui.i_sar_height;
            if (sar > 1.0) {
                nDispWdt *= sar;
            }
            else {
                nDispHgt /= sar;
            }
            pVideoSummary->par_h = stParam.vui.i_sar_width;
            pVideoSummary->par_v = stParam.vui.i_sar_height;
        }
        pVideoSummary->color.primaries_index = stParam.vui.i_colorprim;
        pVideoSummary->color.transfer_index = stParam.vui.i_transfer;
        pVideoSummary->color.matrix_index = stParam.vui.i_colmatrix >= 0 ? stParam.vui.i_colmatrix : ISOM_MATRIX_INDEX_UNSPECIFIED;
        pVideoSummary->color.full_range = stParam.vui.b_fullrange >= 0 ? stParam.vui.b_fullrange : 0;

        lsmash_track_parameters_t stTrackParam;
        lsmash_initialize_track_parameters(&stTrackParam);
        lsmash_track_mode track_mode = (lsmash_track_mode)(ISOM_TRACK_ENABLED | ISOM_TRACK_IN_MOVIE | ISOM_TRACK_IN_PREVIEW);
        stTrackParam.mode = track_mode;
        stTrackParam.display_width = nDispWdt;
        stTrackParam.display_height = nDispHgt;
        VIDEO_ASSERT(lsmash_set_track_parameters(pRoot, nVideoTrack, &stTrackParam) == 0, "failed to set track parameters for video.\n", false);

        lsmash_media_parameters_t stMediaParam;
        lsmash_initialize_media_parameters(&stMediaParam);
        stMediaParam.timescale = (uint32_t)nMediaTimescale;
        stMediaParam.media_handler_name = (char*)"Video(X264)";
        VIDEO_ASSERT(lsmash_set_media_parameters(pRoot, nVideoTrack, &stMediaParam) == 0,
            "failed to set media parameters for video.\n", false);
        nVideoTs = lsmash_get_media_timescale(pRoot, nVideoTrack);
        VIDEO_ASSERT(nVideoTs != 0, "media timescale for video is broken.\n", false);

        x264_nal_t* pNAL;
        int nNAL;
        VIDEO_ASSERT(x264_encoder_headers(pX264Handle, &pNAL, &nNAL) > 0, "x264_encoder_headers", false);
		VIDEO_ASSERT(WriteVideoHeaders(pNAL) == true, "failed to write video headers", false);
		bVideoTrackCreated = true;
        return true;
	}
    return false;
}

bool Video::CreateAudioTrack(void)
{
    if (bAudioTrackCreated == false)
    {
        //open encoder
        //fdk-aac
        if (aacEncOpen(&pAACHandle, 0, 0) != AACENC_OK) {
            return false;
        }

        if (aacEncoder_SetParam(pAACHandle, AACENC_AOT, AOT_AAC_LC) != AACENC_OK) {
            return false;
        }

        if (aacEncoder_SetParam(pAACHandle, AACENC_SAMPLERATE, nAudioSampleRate) != AACENC_OK) {
            return false;
        }

        if (aacEncoder_SetParam(pAACHandle, AACENC_CHANNELMODE, nAudioChannel) != AACENC_OK) {
            return false;
        }

        if (aacEncoder_SetParam(pAACHandle, AACENC_CHANNELORDER, 1) != AACENC_OK) {
            return false;
        }

        if (aacEncoder_SetParam(pAACHandle, AACENC_BITRATE, 320000) != AACENC_OK) {
            return false;
        }

        if (aacEncoder_SetParam(pAACHandle, AACENC_TRANSMUX, TT_MP4_RAW) != AACENC_OK) {
            return false;
        }

        // Initialize encoder
        if (aacEncEncode(pAACHandle, NULL, NULL, NULL, NULL) != AACENC_OK) {
            return false;
        }

        // Get encoder info
        if (aacEncInfo(pAACHandle, &stAACInfo) != AACENC_OK) {
            return false;
        }


        nAudioTrack = lsmash_create_track(pRoot, ISOM_MEDIA_HANDLER_TYPE_AUDIO_TRACK);

		VIDEO_ASSERT(nAudioTrack != 0, "Failed to create audio track", false);

        lsmash_track_parameters_t trackParams;
        lsmash_initialize_track_parameters(&trackParams);
        trackParams.mode =
            lsmash_track_mode(ISOM_TRACK_ENABLED | ISOM_TRACK_IN_MOVIE);
        trackParams.audio_volume = 0x0100;

        // Set track parameters

        VIDEO_ASSERT(lsmash_set_track_parameters(pRoot, nAudioTrack, &trackParams) == 0,
			"Failed to set track parameters for audio", false);

        lsmash_media_parameters_t mediaParams;
        lsmash_initialize_media_parameters(&mediaParams);
        mediaParams.ISO_language = ISOM_LANGUAGE_CODE_UNDEFINED;
        mediaParams.media_handler_name = (char*)"Audio(FDK-AAC)";
        mediaParams.timescale = nAudioSampleRate;

        // Set media parameters

		VIDEO_ASSERT(lsmash_set_media_parameters(pRoot, nAudioTrack, &mediaParams) == 0, "failed set media parameters for audio", false);

        pAudioSummary = (lsmash_audio_summary_t *)lsmash_create_summary(LSMASH_SUMMARY_TYPE_AUDIO);

		VIDEO_ASSERT(pAudioSummary != NULL, "Failed to create audio summary", false);
        pAudioSummary->sample_type = ISOM_CODEC_TYPE_MP4A_AUDIO;
        pAudioSummary->aot = MP4A_AUDIO_OBJECT_TYPE_AAC_LC;
		pAudioSummary->frequency = nAudioSampleRate;
        pAudioSummary->channels = nAudioChannel;
        pAudioSummary->sample_size = 16;
        pAudioSummary->samples_in_frame = nAudioSamples;
        pAudioSummary->bytes_per_frame = 0;

        if (stAACInfo.confSize > 0) {
            lsmash_codec_specific_t* cs = lsmash_create_codec_specific_data(
                LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG,
                LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED);
			VIDEO_ASSERT(cs != NULL, "Failed to create codec specific data for audio", false);
            lsmash_mp4sys_decoder_parameters_t* param = (lsmash_mp4sys_decoder_parameters_t*)cs->data.structured;
            param->objectTypeIndication = MP4SYS_OBJECT_TYPE_Audio_ISO_14496_3;
            param->streamType = MP4SYS_STREAM_TYPE_AudioStream;
            int err = lsmash_set_mp4sys_decoder_specific_info(param, stAACInfo.confBuf, stAACInfo.confSize);
			VIDEO_ASSERT(err == 0, "Failed to set MP4SYS decoder specific info for audio", false);
			err = lsmash_add_codec_specific_data((lsmash_summary_t*)pAudioSummary, cs);
            VIDEO_ASSERT(err == 0, "Failed to add codec specific data for audio", false);
            lsmash_destroy_codec_specific_data(cs);
        } else {
			fprintf(stderr, "Warning: No configuration data for audio encoder.\n");
        }

        nAudioSampleEntry = lsmash_add_sample_entry(pRoot, nAudioTrack, pAudioSummary);
        VIDEO_ASSERT(nAudioSampleEntry != 0, "failed to add sample entry for video.\n", false);
        bAudioTrackCreated = true;

        return true;
    }
    return false;
}

bool Video::WriteVideoHeaders(x264_nal_t* pNAL)
{
    uint32_t sps_size = pNAL[0].i_payload - H264_NALU_LENGTH_SIZE;
    uint32_t pps_size = pNAL[1].i_payload - H264_NALU_LENGTH_SIZE;
    uint32_t sei_size = pNAL[2].i_payload;

    uint8_t* sps = pNAL[0].p_payload + H264_NALU_LENGTH_SIZE;
    uint8_t* pps = pNAL[1].p_payload + H264_NALU_LENGTH_SIZE;
    uint8_t* sei = pNAL[2].p_payload;

    lsmash_codec_specific_t* cs = lsmash_create_codec_specific_data(LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264,
        LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED);

    lsmash_h264_specific_parameters_t* param = (lsmash_h264_specific_parameters_t*)cs->data.structured;
    param->lengthSizeMinusOne = H264_NALU_LENGTH_SIZE - 1;

    /* SPS
     * The remaining parameters are automatically set by SPS. */
    if (lsmash_append_h264_parameter_set(param, H264_PARAMETER_SET_TYPE_SPS, sps, sps_size))
    {
        return -1;
    }

    /* PPS */
    if (lsmash_append_h264_parameter_set(param, H264_PARAMETER_SET_TYPE_PPS, pps, pps_size))
    {
        return -1;
    }

    if (lsmash_add_codec_specific_data((lsmash_summary_t*)pVideoSummary, cs))
    {
        return -1;
    }

    lsmash_destroy_codec_specific_data(cs);

    /* Additional extensions */
    /* Bitrate info */
    cs = lsmash_create_codec_specific_data(LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264_BITRATE,
        LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED);
    if (cs)
        lsmash_add_codec_specific_data((lsmash_summary_t*)pVideoSummary, cs);
    lsmash_destroy_codec_specific_data(cs);

    nVideoSampleEntry = lsmash_add_sample_entry(pRoot, nVideoTrack, pVideoSummary);
    VIDEO_ASSERT(nVideoSampleEntry != 0, "failed to add sample entry for video.\n", false);

    /* SEI */
    pSEIBuffer = new uint8_t[sei_size];
    VIDEO_ASSERT(pSEIBuffer != NULL, "failed to allocate sei transition buffer.\n", false);
    memcpy(pSEIBuffer, sei, sei_size);
    nSEISize = sei_size;

    return sei_size + sps_size + pps_size;
}

uint8_t* Video::GetFrameData(void)
{
    return pRGBBuffer;
}

bool Video::AddFrameInternal(x264_picture_t* pPicture)
{
    x264_picture_t stPicOut;
    x264_nal_t* nal;
    int i_nal;
    int nFrameSize = 0;

    nFrameSize = x264_encoder_encode(pX264Handle, &nal, &i_nal, pPicture, &stPicOut);

    if (nFrameSize < 0) {
        fprintf(stderr, "frame not encode\n");
        return false;
    }

    if (nFrameSize > 0) {
        VIDEO_ASSERT(WriteVideoFrame(nal[0].p_payload, nFrameSize, &stPicOut) == true, "failed write frame", false);
        nLastDTS = stPicOut.i_dts;
        if (nFramesAfterWritten == 0) {
            nFirstDTS = stPicOut.i_dts;
            nLargestPTS1 = nLargestPTS2 = stPicOut.i_pts;
        }
        else {
            nLargestPTS2 = nLargestPTS1;
            nLargestPTS1 = stPicOut.i_pts;
        }
        nFramesAfterWritten++;
    }

    nFrames++;

    return true;
}

bool Video::WriteVideoFrame(uint8_t* pNALU, int nSize, x264_picture_t* pPicture)
{
    uint64_t dts, cts;

    if (!nFramesWritten)
    {
        nStartOffset = pPicture->i_dts * -1;
        nFirstCTS = nStartOffset * nTimeIncrement;
    }

    lsmash_sample_t* pLsmashSample = lsmash_create_sample(nSize + nSEISize);
    VIDEO_ASSERT(pLsmashSample != NULL, "failed to create a video sample data.\n", false);

    if (pSEIBuffer)
    {
        memcpy(pLsmashSample->data, pSEIBuffer, nSEISize);
        free(pSEIBuffer);
        pSEIBuffer = NULL;
    }

    memcpy(pLsmashSample->data + nSEISize, pNALU, nSize);
    nSEISize = 0;

    dts = (pPicture->i_dts + nStartOffset) * nTimeIncrement;
    cts = (pPicture->i_pts + nStartOffset) * nTimeIncrement;

    pLsmashSample->dts = dts;
    pLsmashSample->cts = cts;
    pLsmashSample->index = nVideoSampleEntry;
    pLsmashSample->prop.ra_flags = pPicture->b_keyframe ? ISOM_SAMPLE_RANDOM_ACCESS_FLAG_SYNC : ISOM_SAMPLE_RANDOM_ACCESS_FLAG_NONE;
    /* Append data per sample. */
    VIDEO_ASSERT(lsmash_append_sample(pRoot, nVideoTrack, pLsmashSample) == 0, "failed to append a video frame.\n", false);

    nPrevDTS = dts;
    nFramesWritten++;

    return true;
}

bool Video::AddFrame(void)
{
    uint8_t* yPtr = stPic.img.plane[0];
    uint8_t* uPtr = stPic.img.plane[1];
    uint8_t* vPtr = stPic.img.plane[2];

    for (int y = 0; y < nHeight; y++) {
        for (int x = 0; x < nWidth; x++) {
            size_t px = (y * nWidth * 3) + x * 3;
            double r = pRGBBuffer[px] / 255.0;
            double g = pRGBBuffer[px + 1] / 255.0;
            double b = pRGBBuffer[px + 2] / 255.0;

            double dy = 0.299 * r + 0.578 * g + 0.114 * b;

            *yPtr++ = clampToU8(dy);
            if (y % 2 == 0 && x % 2 == 0) {
                *uPtr++ = clampToU8((b - dy) * 0.565 + 0.5);
                *vPtr++ = clampToU8((r - dy) * 0.713 + 0.5);
            }
        }
    }

    stPic.i_pts = nFrames;
    return AddFrameInternal(&stPic);
}

bool Video::AddAudio(int16_t* pAudioData, uint32_t nSamples)
{
    uint16_t* pEncodedAudio = (uint16_t*)malloc(nSamples * 32);
    if (pAudioBuffer == NULL || nSamples > nAudioSamples) {
        fprintf(stderr, "Invalid audio buffer or sample count.\n");
        return false;
    }
    // Prepare input buffer
    AACENC_BufDesc in_buf_desc = { 0 };
    AACENC_InArgs in_args = { 0 };
    AACENC_BufDesc out_buf_desc = { 0 };
    AACENC_OutArgs out_args = { 0 };

    // Input buffer setup
    int in_identifier = IN_AUDIO_DATA;
    int in_size = nSamples * nAudioChannel * sizeof(int16_t);
    int in_elem_size = sizeof(int16_t);
    void* in_ptr = pAudioData;

    in_buf_desc.numBufs = 1;
    in_buf_desc.bufs = &in_ptr;
    in_buf_desc.bufferIdentifiers = &in_identifier;
    in_buf_desc.bufSizes = &in_size;
    in_buf_desc.bufElSizes = &in_elem_size;

    // Output buffer setup
    int out_identifier = OUT_BITSTREAM_DATA;
    int out_size = nSamples * 32;
    int out_elem_size = sizeof(uint8_t);
    void* out_ptr = pEncodedAudio;

    out_buf_desc.numBufs = 1;
    out_buf_desc.bufs = &out_ptr;
    out_buf_desc.bufferIdentifiers = &out_identifier;
    out_buf_desc.bufSizes = &out_size;
    out_buf_desc.bufElSizes = &out_elem_size;

    // Input arguments
    in_args.numInSamples = nSamples * nAudioChannel; 

    // Encode audio frame
    AACENC_ERROR err = aacEncEncode(pAACHandle, &in_buf_desc, &out_buf_desc, &in_args, &out_args);
    if (err != AACENC_OK) {
        return false;
    }
    lsmash_sample_t* pLsmashSample = lsmash_create_sample(out_args.numOutBytes);
    VIDEO_ASSERT(pLsmashSample != NULL, "failed to create a audio sample data.\n", false);
	memcpy(pLsmashSample->data, pEncodedAudio, out_args.numOutBytes);

    pLsmashSample->dts = nAudioSamplesWritten;
    pLsmashSample->cts = nAudioSamplesWritten;
    pLsmashSample->index = nAudioSampleEntry;

    VIDEO_ASSERT(lsmash_append_sample(pRoot, nAudioTrack, pLsmashSample) == 0, "failed to append a audio frame.\n", false);

    nAudioSamplesWritten += nSamples;

    free(pEncodedAudio);
    return true;
}

bool Video::Close(void)
{
	if (pRoot)
	{
        double dActDuration = 0;
        {
            int64_t last_delta = nLargestPTS1 - nLargestPTS2;
            lsmash_flush_pooled_samples(pRoot, nVideoTrack, (uint32_t)((last_delta ? last_delta : 1) * nTimeIncrement));

            if (nMovieTs != 0 && nVideoTs != 0)    /* avoid zero division */
                dActDuration = ((double)((nLargestPTS1 + last_delta) * nTimeIncrement) / nVideoTs) * nMovieTs;
            else
                printf("timescale is broken.\n");

            lsmash_edit_t stEdit;
            stEdit.duration = dActDuration;
            stEdit.start_time = nFirstCTS;
            stEdit.rate = ISOM_EDIT_MODE_NORMAL;
            VIDEO_ASSERT(lsmash_create_explicit_timeline_map(pRoot, nVideoTrack, stEdit) == 0, "failed to set timeline map for video.\n", false);
            VIDEO_ASSERT(lsmash_modify_explicit_timeline_map(pRoot, nVideoTrack, 1, stEdit) == 0, "failed to update timeline map for video.\n", false);
        }

        if (bAudioTrackCreated) {
            lsmash_flush_pooled_samples(pRoot, nAudioTrack, nAudioSamplesWritten);

            double audioDurationInSeconds = (double)nAudioSamplesWritten / nAudioSampleRate;
            uint64_t audioDurationInMovieTimescale = (uint64_t)(audioDurationInSeconds * nMovieTs);

            uint64_t finalDuration = (audioDurationInMovieTimescale > dActDuration) ?
                audioDurationInMovieTimescale : (uint64_t)dActDuration;

            lsmash_edit_t audio_edit;
            audio_edit.duration = finalDuration;
            audio_edit.start_time = 0;
            audio_edit.rate = ISOM_EDIT_MODE_NORMAL;

            VIDEO_ASSERT(lsmash_create_explicit_timeline_map(pRoot, nAudioTrack, audio_edit) == 0,
                "failed to set timeline map for audio.\n", false);
            VIDEO_ASSERT(lsmash_modify_explicit_timeline_map(pRoot, nAudioTrack, 1, audio_edit) == 0,
                "failed to update timeline map for audio.\n", false);

        }

		VIDEO_ASSERT(lsmash_finish_movie(pRoot, NULL) == 0, "Failed to finish movie", false);
	}

	lsmash_cleanup_summary((lsmash_summary_t*)pVideoSummary);
	lsmash_close_file(&stFileParam);
	lsmash_destroy_root(pRoot);

	free(pSEIBuffer);
	free(pX264Handle);

	pX264Handle = NULL;

    if (bAudioTrackCreated)
    {
        if (pAACHandle)
            aacEncClose(&pAACHandle);
        pAACHandle = NULL;
    }

    return true;
}

void Video::SetParams(uint32_t nDestWidth, uint32_t nDestHeight, uint32_t nDestFrameRate)
{
	int32_t nPrevWidth = nWidth;
	int32_t nPrevHeight = nHeight;
	nWidth = nDestWidth;
	nHeight = nDestHeight;
	nFrameRate = nDestFrameRate;
	//NOTE: the RGB buffer will be lost!
	if (nPrevWidth != nDestWidth && nPrevHeight != nDestHeight)
	{
		if (pRGBBuffer)
			delete[] pRGBBuffer;
		pRGBBuffer = new uint8_t[nWidth * nHeight * 3];
	}
}

void Video::SetParamsAudio(uint32_t nDestSampleRate, uint32_t nSamples, uint32_t nDestChannel)
{
    nAudioSampleRate = nDestSampleRate;
    nAudioChannel = nDestChannel;
	nAudioSamples = nSamples;
    if (pAudioBuffer)
		delete[] pAudioBuffer;
	pAudioBuffer = new int16_t[nAudioSamples * nAudioChannel];
	VIDEO_ASSERT_NORET(pAudioBuffer != NULL, "Failed to allocate audio buffer");
    memset(pAudioBuffer, 0, nAudioSamples * nAudioChannel * sizeof(int16_t));

}

Video::~Video(void)
{
	Close();
	if (pRGBBuffer)
		delete[] pRGBBuffer;
	if (pSEIBuffer)
		delete[] pSEIBuffer;
}