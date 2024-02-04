/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <SDL.h>

#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include "sound.h"

#if defined(CONF_VIDEORECORDER)
#include <engine/shared/video.h>
#endif
extern "C" {
#include <opusfile.h>
#include <wavpack.h>
}

#include <cmath>

static constexpr int SAMPLE_INDEX_USED = -2;
static constexpr int SAMPLE_INDEX_FULL = -1;

void CSound::Mix(short *pFinalOut, unsigned Frames)
{
	Frames = minimum(Frames, m_MaxFrames);
	mem_zero(m_pMixBuffer, Frames * 2 * sizeof(int));

	// acquire lock while we are mixing
	m_SoundLock.lock();

	const int MasterVol = m_SoundVolume.load(std::memory_order_relaxed);

	for(auto &Voice : m_aVoices)
	{
		if(!Voice.m_pSample)
			continue;

		// mix voice
		int *pOut = m_pMixBuffer;

		const int Step = Voice.m_pSample->m_Channels; // setup input sources
		short *pInL = &Voice.m_pSample->m_pData[Voice.m_Tick * Step];
		short *pInR = &Voice.m_pSample->m_pData[Voice.m_Tick * Step + 1];

		unsigned End = Voice.m_pSample->m_NumFrames - Voice.m_Tick;

		int VolumeR = round_truncate(Voice.m_pChannel->m_Vol * (Voice.m_Vol / 255.0f));
		int VolumeL = VolumeR;

		// make sure that we don't go outside the sound data
		if(Frames < End)
			End = Frames;

		// check if we have a mono sound
		if(Voice.m_pSample->m_Channels == 1)
			pInR = pInL;

		// volume calculation
		if(Voice.m_Flags & ISound::FLAG_POS && Voice.m_pChannel->m_Pan)
		{
			// TODO: we should respect the channel panning value
			const int dx = Voice.m_X - m_CenterX.load(std::memory_order_relaxed);
			const int dy = Voice.m_Y - m_CenterY.load(std::memory_order_relaxed);
			float FalloffX = 0.0f;
			float FalloffY = 0.0f;

			int RangeX = 0; // for panning
			bool InVoiceField = false;

			switch(Voice.m_Shape)
			{
			case ISound::SHAPE_CIRCLE:
			{
				const float Radius = Voice.m_Circle.m_Radius;
				RangeX = Radius;

				// dx and dy can be larger than 46341 and thus the calculation would go beyond the limits of a integer,
				// therefore we cast them into float
				const int Dist = (int)length(vec2(dx, dy));
				if(Dist < Radius)
				{
					InVoiceField = true;

					// falloff
					int FalloffDistance = Radius * Voice.m_Falloff;
					if(Dist > FalloffDistance)
						FalloffX = FalloffY = (Radius - Dist) / (Radius - FalloffDistance);
					else
						FalloffX = FalloffY = 1.0f;
				}
				else
					InVoiceField = false;

				break;
			}

			case ISound::SHAPE_RECTANGLE:
			{
				RangeX = Voice.m_Rectangle.m_Width / 2.0f;

				const int abs_dx = absolute(dx);
				const int abs_dy = absolute(dy);

				const int w = Voice.m_Rectangle.m_Width / 2.0f;
				const int h = Voice.m_Rectangle.m_Height / 2.0f;

				if(abs_dx < w && abs_dy < h)
				{
					InVoiceField = true;

					// falloff
					int fx = Voice.m_Falloff * w;
					int fy = Voice.m_Falloff * h;

					FalloffX = abs_dx > fx ? (float)(w - abs_dx) / (w - fx) : 1.0f;
					FalloffY = abs_dy > fy ? (float)(h - abs_dy) / (h - fy) : 1.0f;
				}
				else
					InVoiceField = false;

				break;
			}
			};

			if(InVoiceField)
			{
				// panning
				if(!(Voice.m_Flags & ISound::FLAG_NO_PANNING))
				{
					if(dx > 0)
						VolumeL = ((RangeX - absolute(dx)) * VolumeL) / RangeX;
					else
						VolumeR = ((RangeX - absolute(dx)) * VolumeR) / RangeX;
				}

				{
					VolumeL *= FalloffX * FalloffY;
					VolumeR *= FalloffX * FalloffY;
				}
			}
			else
			{
				VolumeL = 0;
				VolumeR = 0;
			}
		}

		// process all frames
		for(unsigned s = 0; s < End; s++)
		{
			*pOut++ += (*pInL) * VolumeL;
			*pOut++ += (*pInR) * VolumeR;
			pInL += Step;
			pInR += Step;
			Voice.m_Tick++;
		}

		// free voice if not used any more
		if(Voice.m_Tick == Voice.m_pSample->m_NumFrames)
		{
			if(Voice.m_Flags & ISound::FLAG_LOOP)
				Voice.m_Tick = 0;
			else
			{
				Voice.m_pSample = nullptr;
				Voice.m_Age++;
			}
		}
	}

	m_SoundLock.unlock();

	// clamp accumulated values
	for(unsigned i = 0; i < Frames * 2; i++)
		pFinalOut[i] = clamp<int>(((m_pMixBuffer[i] * MasterVol) / 101) >> 8, std::numeric_limits<short>::min(), std::numeric_limits<short>::max());

#if defined(CONF_ARCH_ENDIAN_BIG)
	swap_endian(pFinalOut, sizeof(short), Frames * 2);
#endif
}

static void SdlCallback(void *pUser, Uint8 *pStream, int Len)
{
	CSound *pSound = static_cast<CSound *>(pUser);

#if defined(CONF_VIDEORECORDER)
	if(!(IVideo::Current() && g_Config.m_ClVideoSndEnable))
	{
		pSound->Mix((short *)pStream, Len / sizeof(short) / 2);
	}
	else
	{
		mem_zero(pStream, Len);
	}
#else
	pSound->Mix((short *)pStream, Len / sizeof(short) / 2);
#endif
}

int CSound::Init()
{
	m_SoundEnabled = false;
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	if(!g_Config.m_SndEnable)
		return 0;

	if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		dbg_msg("sound", "unable to init SDL audio: %s", SDL_GetError());
		return -1;
	}

	m_MixingRate = g_Config.m_SndRate;

	SDL_AudioSpec Format, FormatOut;
	Format.freq = m_MixingRate;
	Format.format = AUDIO_S16;
	Format.channels = 2;
	Format.samples = g_Config.m_SndBufferSize;
	Format.callback = SdlCallback;
	Format.userdata = this;

	// Open the audio device and start playing sound!
	m_Device = SDL_OpenAudioDevice(nullptr, 0, &Format, &FormatOut, 0);

	if(m_Device == 0)
	{
		dbg_msg("sound", "unable to open audio: %s", SDL_GetError());
		return -1;
	}
	else
		dbg_msg("sound", "sound init successful using audio driver '%s'", SDL_GetCurrentAudioDriver());

	m_MaxFrames = FormatOut.samples * 2;
#if defined(CONF_VIDEORECORDER)
	m_MaxFrames = maximum<uint32_t>(m_MaxFrames, 1024 * 2); // make the buffer bigger just in case
#endif
	m_pMixBuffer = (int *)calloc(m_MaxFrames * 2, sizeof(int));

	m_FirstFreeSampleIndex = 0;
	for(size_t i = 0; i < std::size(m_aSamples) - 1; ++i)
	{
		m_aSamples[i].m_Index = i;
		m_aSamples[i].m_NextFreeSampleIndex = i + 1;
	}
	m_aSamples[std::size(m_aSamples) - 1].m_Index = std::size(m_aSamples) - 1;
	m_aSamples[std::size(m_aSamples) - 1].m_NextFreeSampleIndex = SAMPLE_INDEX_FULL;

	SDL_PauseAudioDevice(m_Device, 0);

	m_SoundEnabled = true;
	Update();
	return 0;
}

int CSound::Update()
{
	UpdateVolume();
	return 0;
}

void CSound::UpdateVolume()
{
	int WantedVolume = g_Config.m_SndVolume;
	if(!m_pGraphics->WindowActive() && g_Config.m_SndNonactiveMute)
		WantedVolume = 0;
	m_SoundVolume.store(WantedVolume, std::memory_order_relaxed);
}

void CSound::Shutdown()
{
	for(unsigned SampleID = 0; SampleID < NUM_SAMPLES; SampleID++)
	{
		UnloadSample(SampleID);
	}

	SDL_CloseAudioDevice(m_Device);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	free(m_pMixBuffer);
	m_pMixBuffer = nullptr;
}

CSample *CSound::AllocSample()
{
	if(m_FirstFreeSampleIndex == SAMPLE_INDEX_FULL)
		return nullptr;

	CSample *pSample = &m_aSamples[m_FirstFreeSampleIndex];
	m_FirstFreeSampleIndex = pSample->m_NextFreeSampleIndex;
	pSample->m_NextFreeSampleIndex = SAMPLE_INDEX_USED;
	if(pSample->m_pData != nullptr)
	{
		char aError[64];
		str_format(aError, sizeof(aError), "Sample was not unloaded (index=%d, duration=%f)", pSample->m_Index, pSample->TotalTime());
		dbg_assert(false, aError);
	}
	return pSample;
}

void CSound::RateConvert(CSample &Sample) const
{
	dbg_assert(Sample.m_pData != nullptr, "Sample is not loaded");
	// make sure that we need to convert this sound
	if(Sample.m_Rate == m_MixingRate)
		return;

	// allocate new data
	const int NumFrames = (int)((Sample.m_NumFrames / (float)Sample.m_Rate) * m_MixingRate);
	short *pNewData = (short *)calloc((size_t)NumFrames * Sample.m_Channels, sizeof(short));

	for(int i = 0; i < NumFrames; i++)
	{
		// resample TODO: this should be done better, like linear at least
		float a = i / (float)NumFrames;
		int f = (int)(a * Sample.m_NumFrames);
		if(f >= Sample.m_NumFrames)
			f = Sample.m_NumFrames - 1;

		// set new data
		if(Sample.m_Channels == 1)
			pNewData[i] = Sample.m_pData[f];
		else if(Sample.m_Channels == 2)
		{
			pNewData[i * 2] = Sample.m_pData[f * 2];
			pNewData[i * 2 + 1] = Sample.m_pData[f * 2 + 1];
		}
	}

	// free old data and apply new
	free(Sample.m_pData);
	Sample.m_pData = pNewData;
	Sample.m_NumFrames = NumFrames;
	Sample.m_Rate = m_MixingRate;
}

bool CSound::DecodeOpus(CSample &Sample, const void *pData, unsigned DataSize) const
{
	OggOpusFile *pOpusFile = op_open_memory((const unsigned char *)pData, DataSize, nullptr);
	if(pOpusFile)
	{
		const int NumChannels = op_channel_count(pOpusFile, -1);
		const int NumSamples = op_pcm_total(pOpusFile, -1); // per channel!

		Sample.m_Channels = NumChannels;

		if(Sample.m_Channels > 2)
		{
			dbg_msg("sound/opus", "file is not mono or stereo.");
			return false;
		}

		Sample.m_pData = (short *)calloc((size_t)NumSamples * NumChannels, sizeof(short));

		int Pos = 0;
		while(Pos < NumSamples)
		{
			const int Read = op_read(pOpusFile, Sample.m_pData + Pos * NumChannels, NumSamples * NumChannels, nullptr);
			if(Read < 0)
			{
				free(Sample.m_pData);
				dbg_msg("sound/opus", "op_read error %d at %d", Read, Pos);
				return false;
			}
			else if(Read == 0) // EOF
				break;
			Pos += Read;
		}

		Sample.m_NumFrames = Pos;
		Sample.m_Rate = 48000;
		Sample.m_LoopStart = -1;
		Sample.m_LoopEnd = -1;
		Sample.m_PausedAt = 0;
	}
	else
	{
		dbg_msg("sound/opus", "failed to decode sample");
		return false;
	}

	return true;
}

// TODO: Update WavPack to get rid of these global variables
static const void *s_pWVBuffer = nullptr;
static int s_WVBufferPosition = 0;
static int s_WVBufferSize = 0;

static int ReadDataOld(void *pBuffer, int Size)
{
	int ChunkSize = minimum(Size, s_WVBufferSize - s_WVBufferPosition);
	mem_copy(pBuffer, (const char *)s_pWVBuffer + s_WVBufferPosition, ChunkSize);
	s_WVBufferPosition += ChunkSize;
	return ChunkSize;
}

#if defined(CONF_WAVPACK_OPEN_FILE_INPUT_EX)
static int ReadData(void *pId, void *pBuffer, int Size)
{
	(void)pId;
	return ReadDataOld(pBuffer, Size);
}

static int ReturnFalse(void *pId)
{
	(void)pId;
	return 0;
}

static unsigned int GetPos(void *pId)
{
	(void)pId;
	return s_WVBufferPosition;
}

static unsigned int GetLength(void *pId)
{
	(void)pId;
	return s_WVBufferSize;
}

static int PushBackByte(void *pId, int Char)
{
	s_WVBufferPosition -= 1;
	return 0;
}
#endif

bool CSound::DecodeWV(CSample &Sample, const void *pData, unsigned DataSize) const
{
	char aError[100];

	dbg_assert(s_pWVBuffer == nullptr, "DecodeWV already in use");
	s_pWVBuffer = pData;
	s_WVBufferSize = DataSize;
	s_WVBufferPosition = 0;

#if defined(CONF_WAVPACK_OPEN_FILE_INPUT_EX)
	WavpackStreamReader Callback = {0};
	Callback.can_seek = ReturnFalse;
	Callback.get_length = GetLength;
	Callback.get_pos = GetPos;
	Callback.push_back_byte = PushBackByte;
	Callback.read_bytes = ReadData;
	WavpackContext *pContext = WavpackOpenFileInputEx(&Callback, (void *)1, 0, aError, 0, 0);
#else
	WavpackContext *pContext = WavpackOpenFileInput(ReadDataOld, aError);
#endif
	if(pContext)
	{
		const int NumSamples = WavpackGetNumSamples(pContext);
		const int BitsPerSample = WavpackGetBitsPerSample(pContext);
		const unsigned int SampleRate = WavpackGetSampleRate(pContext);
		const int NumChannels = WavpackGetNumChannels(pContext);

		Sample.m_Channels = NumChannels;
		Sample.m_Rate = SampleRate;

		if(Sample.m_Channels > 2)
		{
			dbg_msg("sound/wv", "file is not mono or stereo.");
			s_pWVBuffer = nullptr;
			return false;
		}

		if(BitsPerSample != 16)
		{
			dbg_msg("sound/wv", "bps is %d, not 16", BitsPerSample);
			s_pWVBuffer = nullptr;
			return false;
		}

		int *pBuffer = (int *)calloc((size_t)NumSamples * NumChannels, sizeof(int));
		if(!WavpackUnpackSamples(pContext, pBuffer, NumSamples))
		{
			free(pBuffer);
			dbg_msg("sound/wv", "WavpackUnpackSamples failed. NumSamples=%d, NumChannels=%d", NumSamples, NumChannels);
			s_pWVBuffer = nullptr;
			return false;
		}

		Sample.m_pData = (short *)calloc((size_t)NumSamples * NumChannels, sizeof(short));

		int *pSrc = pBuffer;
		short *pDst = Sample.m_pData;
		for(int i = 0; i < NumSamples * NumChannels; i++)
			*pDst++ = (short)*pSrc++;

		free(pBuffer);
#ifdef CONF_WAVPACK_CLOSE_FILE
		WavpackCloseFile(pContext);
#endif

		Sample.m_NumFrames = NumSamples;
		Sample.m_LoopStart = -1;
		Sample.m_LoopEnd = -1;
		Sample.m_PausedAt = 0;

		s_pWVBuffer = nullptr;
	}
	else
	{
		dbg_msg("sound/wv", "failed to decode sample (%s)", aError);
		s_pWVBuffer = nullptr;
		return false;
	}

	return true;
}

int CSound::LoadOpus(const char *pFilename, int StorageType)
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled)
		return -1;

	if(!m_pStorage)
		return -1;

	CSample *pSample = AllocSample();
	if(!pSample)
	{
		dbg_msg("sound/opus", "failed to allocate sample ID. filename='%s'", pFilename);
		return -1;
	}

	void *pData;
	unsigned DataSize;
	if(!m_pStorage->ReadFile(pFilename, StorageType, &pData, &DataSize))
	{
		UnloadSample(pSample->m_Index);
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	const bool DecodeSuccess = DecodeOpus(*pSample, pData, DataSize);
	free(pData);
	if(!DecodeSuccess)
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	if(g_Config.m_Debug)
		dbg_msg("sound/opus", "loaded %s", pFilename);

	RateConvert(*pSample);
	return pSample->m_Index;
}

int CSound::LoadWV(const char *pFilename, int StorageType)
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled)
		return -1;

	if(!m_pStorage)
		return -1;

	CSample *pSample = AllocSample();
	if(!pSample)
	{
		dbg_msg("sound/wv", "failed to allocate sample ID. filename='%s'", pFilename);
		return -1;
	}

	void *pData;
	unsigned DataSize;
	if(!m_pStorage->ReadFile(pFilename, StorageType, &pData, &DataSize))
	{
		UnloadSample(pSample->m_Index);
		dbg_msg("sound/wv", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	const bool DecodeSuccess = DecodeWV(*pSample, pData, DataSize);
	free(pData);
	if(!DecodeSuccess)
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	if(g_Config.m_Debug)
		dbg_msg("sound/wv", "loaded %s", pFilename);

	RateConvert(*pSample);
	return pSample->m_Index;
}

int CSound::LoadOpusFromMem(const void *pData, unsigned DataSize, bool FromEditor = false)
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled && !FromEditor)
		return -1;

	if(!pData)
		return -1;

	CSample *pSample = AllocSample();
	if(!pSample)
		return -1;

	if(!DecodeOpus(*pSample, pData, DataSize))
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	RateConvert(*pSample);
	return pSample->m_Index;
}

int CSound::LoadWVFromMem(const void *pData, unsigned DataSize, bool FromEditor = false)
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled && !FromEditor)
		return -1;

	if(!pData)
		return -1;

	CSample *pSample = AllocSample();
	if(!pSample)
		return -1;

	if(!DecodeWV(*pSample, pData, DataSize))
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	RateConvert(*pSample);
	return pSample->m_Index;
}

void CSound::UnloadSample(int SampleID)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return;

	Stop(SampleID);

	// Free data
	CSample &Sample = m_aSamples[SampleID];
	free(Sample.m_pData);
	Sample.m_pData = nullptr;

	// Free slot
	if(Sample.m_NextFreeSampleIndex == SAMPLE_INDEX_USED)
	{
		Sample.m_NextFreeSampleIndex = m_FirstFreeSampleIndex;
		m_FirstFreeSampleIndex = Sample.m_Index;
	}
}

float CSound::GetSampleTotalTime(int SampleID)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return 0.0f;

	return m_aSamples[SampleID].TotalTime();
}

float CSound::GetSampleCurrentTime(int SampleID)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return 0.0f;

	const CLockScope LockScope(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleID];
	for(auto &Voice : m_aVoices)
	{
		if(Voice.m_pSample == pSample)
		{
			return Voice.m_Tick / (float)pSample->m_Rate;
		}
	}

	return pSample->m_PausedAt / (float)pSample->m_Rate;
}

void CSound::SetSampleCurrentTime(int SampleID, float Time)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return;

	const CLockScope LockScope(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleID];
	for(auto &Voice : m_aVoices)
	{
		if(Voice.m_pSample == pSample)
		{
			Voice.m_Tick = pSample->m_NumFrames * Time;
			return;
		}
	}

	pSample->m_PausedAt = pSample->m_NumFrames * Time;
}

void CSound::SetChannel(int ChannelID, float Vol, float Pan)
{
	m_aChannels[ChannelID].m_Vol = (int)(Vol * 255.0f);
	m_aChannels[ChannelID].m_Pan = (int)(Pan * 255.0f); // TODO: this is only on and off right now
}

void CSound::SetListenerPos(float x, float y)
{
	m_CenterX.store((int)x, std::memory_order_relaxed);
	m_CenterY.store((int)y, std::memory_order_relaxed);
}

void CSound::SetVoiceVolume(CVoiceHandle Voice, float Volume)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	Volume = clamp(Volume, 0.0f, 1.0f);
	m_aVoices[VoiceID].m_Vol = (int)(Volume * 255.0f);
}

void CSound::SetVoiceFalloff(CVoiceHandle Voice, float Falloff)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	Falloff = clamp(Falloff, 0.0f, 1.0f);
	m_aVoices[VoiceID].m_Falloff = Falloff;
}

void CSound::SetVoiceLocation(CVoiceHandle Voice, float x, float y)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceID].m_X = x;
	m_aVoices[VoiceID].m_Y = y;
}

void CSound::SetVoiceTimeOffset(CVoiceHandle Voice, float TimeOffset)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	if(!m_aVoices[VoiceID].m_pSample)
		return;

	int Tick = 0;
	bool IsLooping = m_aVoices[VoiceID].m_Flags & ISound::FLAG_LOOP;
	uint64_t TickOffset = m_aVoices[VoiceID].m_pSample->m_Rate * TimeOffset;
	if(m_aVoices[VoiceID].m_pSample->m_NumFrames > 0 && IsLooping)
		Tick = TickOffset % m_aVoices[VoiceID].m_pSample->m_NumFrames;
	else
		Tick = clamp(TickOffset, (uint64_t)0, (uint64_t)m_aVoices[VoiceID].m_pSample->m_NumFrames);

	// at least 200msec off, else depend on buffer size
	float Threshold = maximum(0.2f * m_aVoices[VoiceID].m_pSample->m_Rate, (float)m_MaxFrames);
	if(absolute(m_aVoices[VoiceID].m_Tick - Tick) > Threshold)
	{
		// take care of looping (modulo!)
		if(!(IsLooping && (minimum(m_aVoices[VoiceID].m_Tick, Tick) + m_aVoices[VoiceID].m_pSample->m_NumFrames - maximum(m_aVoices[VoiceID].m_Tick, Tick)) <= Threshold))
		{
			m_aVoices[VoiceID].m_Tick = Tick;
		}
	}
}

void CSound::SetVoiceCircle(CVoiceHandle Voice, float Radius)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceID].m_Shape = ISound::SHAPE_CIRCLE;
	m_aVoices[VoiceID].m_Circle.m_Radius = maximum(0.0f, Radius);
}

void CSound::SetVoiceRectangle(CVoiceHandle Voice, float Width, float Height)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceID].m_Shape = ISound::SHAPE_RECTANGLE;
	m_aVoices[VoiceID].m_Rectangle.m_Width = maximum(0.0f, Width);
	m_aVoices[VoiceID].m_Rectangle.m_Height = maximum(0.0f, Height);
}

ISound::CVoiceHandle CSound::Play(int ChannelID, int SampleID, int Flags, float x, float y)
{
	const CLockScope LockScope(m_SoundLock);

	// search for voice
	int VoiceID = -1;
	for(int i = 0; i < NUM_VOICES; i++)
	{
		int NextID = (m_NextVoice + i) % NUM_VOICES;
		if(!m_aVoices[NextID].m_pSample)
		{
			VoiceID = NextID;
			m_NextVoice = NextID + 1;
			break;
		}
	}

	// voice found, use it
	int Age = -1;
	if(VoiceID != -1)
	{
		m_aVoices[VoiceID].m_pSample = &m_aSamples[SampleID];
		m_aVoices[VoiceID].m_pChannel = &m_aChannels[ChannelID];
		if(Flags & FLAG_LOOP)
		{
			m_aVoices[VoiceID].m_Tick = m_aSamples[SampleID].m_PausedAt;
		}
		else if(Flags & FLAG_PREVIEW)
		{
			m_aVoices[VoiceID].m_Tick = m_aSamples[SampleID].m_PausedAt;
			m_aSamples[SampleID].m_PausedAt = 0;
		}
		else
		{
			m_aVoices[VoiceID].m_Tick = 0;
		}
		m_aVoices[VoiceID].m_Vol = 255;
		m_aVoices[VoiceID].m_Flags = Flags;
		m_aVoices[VoiceID].m_X = (int)x;
		m_aVoices[VoiceID].m_Y = (int)y;
		m_aVoices[VoiceID].m_Falloff = 0.0f;
		m_aVoices[VoiceID].m_Shape = ISound::SHAPE_CIRCLE;
		m_aVoices[VoiceID].m_Circle.m_Radius = 1500;
		Age = m_aVoices[VoiceID].m_Age;
	}

	return CreateVoiceHandle(VoiceID, Age);
}

ISound::CVoiceHandle CSound::PlayAt(int ChannelID, int SampleID, int Flags, float x, float y)
{
	return Play(ChannelID, SampleID, Flags | ISound::FLAG_POS, x, y);
}

ISound::CVoiceHandle CSound::Play(int ChannelID, int SampleID, int Flags)
{
	return Play(ChannelID, SampleID, Flags, 0, 0);
}

void CSound::Pause(int SampleID)
{
	// TODO: a nice fade out
	const CLockScope LockScope(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleID];
	for(auto &Voice : m_aVoices)
	{
		if(Voice.m_pSample == pSample)
		{
			Voice.m_pSample->m_PausedAt = Voice.m_Tick;
			Voice.m_pSample = nullptr;
		}
	}
}

void CSound::Stop(int SampleID)
{
	// TODO: a nice fade out
	const CLockScope LockScope(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleID];
	for(auto &Voice : m_aVoices)
	{
		if(Voice.m_pSample == pSample)
		{
			if(Voice.m_Flags & FLAG_LOOP)
				Voice.m_pSample->m_PausedAt = Voice.m_Tick;
			else
				Voice.m_pSample->m_PausedAt = 0;
			Voice.m_pSample = nullptr;
		}
	}
}

void CSound::StopAll()
{
	// TODO: a nice fade out
	const CLockScope LockScope(m_SoundLock);
	for(auto &Voice : m_aVoices)
	{
		if(Voice.m_pSample)
		{
			if(Voice.m_Flags & FLAG_LOOP)
				Voice.m_pSample->m_PausedAt = Voice.m_Tick;
			else
				Voice.m_pSample->m_PausedAt = 0;
		}
		Voice.m_pSample = nullptr;
	}
}

void CSound::StopVoice(CVoiceHandle Voice)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceID].m_pSample = nullptr;
	m_aVoices[VoiceID].m_Age++;
}

bool CSound::IsPlaying(int SampleID)
{
	const CLockScope LockScope(m_SoundLock);
	const CSample *pSample = &m_aSamples[SampleID];
	return std::any_of(std::begin(m_aVoices), std::end(m_aVoices), [pSample](const auto &Voice) { return Voice.m_pSample == pSample; });
}

void CSound::PauseAudioDevice()
{
	SDL_PauseAudioDevice(m_Device, 1);
}

void CSound::UnpauseAudioDevice()
{
	SDL_PauseAudioDevice(m_Device, 0);
}

IEngineSound *CreateEngineSound() { return new CSound; }
