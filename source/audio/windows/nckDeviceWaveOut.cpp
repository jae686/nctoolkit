
/**
 * NCtoolKit © 2007-2015 Luís F.Loureiro, under zlib software license.
 * https://github.com/nczeroshift/nctoolkit
 */

#include "nckDeviceWaveOut.h"
#include "../nckOggStream.h"

#if defined(NCK_WAVEOUT)

_AUDIO_BEGIN

DeviceWaveOut::DeviceWaveOut() : Threadable() {
	m_Device = NULL;
	m_SampleRate = 44100;
	m_Channels = 2;
	m_Format = FORMAT_S16;
	memset(&m_Device,0,sizeof(HWAVEOUT));
}

DeviceWaveOut::DeviceWaveOut(int sampleRate, int channels, Format format,int bufferSize, int bufferCount) : Threadable() {
	m_Device = NULL;
	m_SampleRate = sampleRate;
	m_Channels = channels;
	m_Format = format;
	m_BufferCount = bufferCount;
	m_BufferSize = bufferSize;
	memset(&m_Device,0,sizeof(HWAVEOUT));
}

DeviceWaveOut::~DeviceWaveOut(){
	Stop();
	for(int i = 0;i < m_Buffers.size(); i++){
		WAVEHDR * wh = m_Buffers[i];
		SafeDelete(wh->lpData);
	}
	waveOutClose(m_Device);
}
	
int DeviceWaveOut::GetSampleRate(){
	return m_SampleRate;
}

int DeviceWaveOut::GetChannelsCount(){
	return m_Channels;
}

Format DeviceWaveOut::GetDataFormat(){
	return m_Format;
}

State DeviceWaveOut::GetState(){
	return m_CurrentState;
}

void DeviceWaveOut::Create(){
	int block = 0;
	int bits = 0;
	GetBlockAndBitsPerSample(m_Format,m_Channels,&block,&bits);

	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(wfx));
	wfx.wFormatTag      = WAVE_FORMAT_PCM;
	wfx.nChannels       = m_Channels;
	wfx.nSamplesPerSec  = m_SampleRate;
	wfx.nAvgBytesPerSec = m_SampleRate * block;
	wfx.nBlockAlign     = block;
	wfx.wBitsPerSample  = bits;
	wfx.cbSize          = sizeof(WAVEFORMATEX);

    MMRESULT result = waveOutOpen(&m_Device, WAVE_MAPPER, &wfx, 0, 0, 0);
    if (result != MMSYSERR_NOERROR) {
		THROW_EXCEPTION("Error starting waveout");
    }

	m_Buffers.reserve(m_BufferCount);
	
	const size_t maxSize = m_SampleRate*block;

	for(int i = 0;i<m_BufferCount;i++)
	{
		WAVEHDR * wh = new WAVEHDR();
		m_Buffers.push_back(wh);
		memset(wh, 0, sizeof(wh));

		wh->lpData = new char[maxSize];
		wh->dwBufferLength = maxSize;
		memset(wh->lpData,0,maxSize);
	}
}

void DeviceWaveOut::Play(){
	if(IsRunning())
	{
		if(m_CurrentState == STATE_PAUSED){
			// Resume playback.
			waveOutRestart(m_Device);
			m_CurrentState = STATE_PLAYING;
		}

		return;
	}

	// Prime buffers ?
	for(int i = 0;i<m_BufferCount;i++)
	{
		WAVEHDR * wh = m_Buffers[i];

		MMRESULT result = waveOutPrepareHeader(m_Device, wh, sizeof(WAVEHDR));
		if (result != MMSYSERR_NOERROR) {
			THROW_EXCEPTION("Error preparing header");
		}

		result = waveOutWrite(m_Device, wh, sizeof(WAVEHDR));
		if (result != MMSYSERR_NOERROR) {
			THROW_EXCEPTION("Error writing data");
		}
	}

    m_CurrentTime = 0;
	Start();
	m_CurrentState = STATE_PLAYING;

}

void DeviceWaveOut::Stop(){
	TearDownAndJoin();

	waveOutReset(m_Device);
	for(int i = 0;i < m_Buffers.size(); i++){
		WAVEHDR * wh = m_Buffers[i];
		if (wh->dwFlags & WHDR_PREPARED || wh->dwFlags & WHDR_DONE) {
			waveOutUnprepareHeader(m_Device, wh, sizeof(wh));
		}
	}

	m_CurrentState = STATE_STOPPED;
}

void DeviceWaveOut::Pause(){
	if(m_CurrentState == STATE_PLAYING){
		m_CurrentState = STATE_PAUSED;
		waveOutPause(m_Device);
	}
}

void DeviceWaveOut::Run(){
	while(!IsTearingDown())
    {
        int block,bits;
		GetBlockAndBitsPerSample(m_Format, m_Channels,&block,&bits);

		if(m_CurrentState == STATE_PLAYING)
        {
			for(int i = 0;i < m_Buffers.size(); i++){
				WAVEHDR * wh = m_Buffers[i];
				if (wh->dwFlags & WHDR_DONE) 
				{
					// unprepare
					MMRESULT result = waveOutUnprepareHeader(m_Device, wh, sizeof(WAVEHDR));
					if (result != MMSYSERR_NOERROR) {
						Core::DebugLog("Error unpreparing header\n");
					}

					if(m_Callback!=NULL)
						wh->dwBufferLength = m_Callback->Buffer(this,wh->dwBufferLength,(uint8_t*)wh->lpData);
				
					if(!wh->dwBufferLength)
						break;

					long double bufferTime = 1e6 * wh->dwBufferLength / (long double)(m_SampleRate * block); // microseconds

					wh->dwFlags = 0;
					result = waveOutPrepareHeader(m_Device, wh, sizeof(WAVEHDR));
					if (result != MMSYSERR_NOERROR) {
						Core::DebugLog("Error preparing header");
					}

					result = waveOutWrite(m_Device, wh, sizeof(WAVEHDR));
					if (result != MMSYSERR_NOERROR) {
						Core::DebugLog("Error writing data");
					}
                    
                    m_CurrentTime += (int64_t)bufferTime;
				}
			}
		}
        Core::Thread::Wait(1); 
	}
}

Stream * DeviceWaveOut::LoadStream(const std::string & filename){
	return OggStream::Load(filename);
}

Stream * DeviceWaveOut::LoadStreamFromReader(Core::DataReader * reader){
	return OggStream::Load(reader);
}

_AUDIO_END

#endif // #if defined(NCK_WAVEOUT)
