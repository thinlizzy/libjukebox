/*
 * DirectSoundBuffer.cpp
 *
 *  Created on: 13 de dez de 2017
 *      Author: rvelloso
 */

#include <string.h>

#include "DirectSoundBuffer.h"

namespace jukebox {

class DSoundDevice {
public:
	DSoundDevice() {
		// create device
		auto hr = DirectSoundCreate(NULL, &lpds, NULL);

		if (FAILED(hr))
			throw std::runtime_error("failed DirectSoundCreate");

		// set cooperative level to priority
		hr = lpds->SetCooperativeLevel(GetConsoleWindow(), DSSCL_PRIORITY);

		if (FAILED(hr))
			throw std::runtime_error("failed SetCooperativeLevel");
	}

	struct IDirectSound &getDevice() {
		return *lpds;
	}
private:
	LPDIRECTSOUND lpds;
} directSoundDevice;

void ReleaseBuffer(LPDIRECTSOUNDBUFFER pDsb) {
	if (pDsb != nullptr)
		pDsb->Release();
};

DirectSoundBuffer::DirectSoundBuffer(SoundFile &file) :
	soundFile(file),
	pDsb(nullptr, ReleaseBuffer) {

	prepare();
}

void DirectSoundBuffer::play() {
	auto hr = pDsb->Play(
	    0,	// Unused.
	    0,	// Priority for voice management.
	    0);	// Flags.

	if (FAILED(hr))
		throw std::runtime_error("failed Play");
}

void DirectSoundBuffer::stop() {
	auto hr = pDsb->Stop();
	if (FAILED(hr))
		throw std::runtime_error("failed Stop");
}

void DirectSoundBuffer::prepare() {
	memset((void *)&wfx, 0, sizeof(wfx));
	memset((void *)&dsbdesc, 0, sizeof(dsbdesc));

	wfx.wBitsPerSample = soundFile.getBitsPerSample();
	wfx.nChannels = soundFile.getNumChannels();
	wfx.nSamplesPerSec = soundFile.getSampleRate();
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nBlockAlign = (wfx.wBitsPerSample/8)*wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags =
			DSBCAPS_CTRLPAN |
			DSBCAPS_CTRLVOLUME |
			DSBCAPS_CTRLFREQUENCY |
			DSBCAPS_GLOBALFOCUS;
	/*
	 * DSBCAPS_CTRL3D				The sound source can be moved in 3D space.
	 * DSBCAPS_CTRLFX				Effects can be added to the buffer.
	 * DSBCAPS_CTRLFREQUENCY		The frequency of the sound can be changed.
	 * DSBCAPS_CTRLPAN				The sound source can be moved from left to right.
	 * DSBCAPS_CTRLPOSITIONNOTIFY	Notification positions can be set on the buffer.
	 * DSBCAPS_CTRLVOLUME			The volume of the sound can be changed.
	 */

	dsbdesc.dwBufferBytes = soundFile.getDataSize();
	dsbdesc.lpwfxFormat = &wfx;

	LPDIRECTSOUNDBUFFER bufPtr;
	auto hr = directSoundDevice.getDevice().CreateSoundBuffer(&dsbdesc, &bufPtr, NULL);

	if (FAILED(hr))
		throw std::runtime_error("failed CreateSoundBuffer");

	pDsb.reset(bufPtr);

	// fill secondary buffer with wav/sound
	LPVOID bufAddr;
	DWORD bufLen;

	hr = pDsb->Lock(
	      0,		// Offset at which to start lock.
	      0,		// Size of lock; ignored because of flag.
	      &bufAddr,	// Gets address of first part of lock.
	      &bufLen,	// Gets size of first part of lock.
	      NULL,		// Address of wraparound not needed.
	      NULL,		// Size of wraparound not needed.
	      DSBLOCK_ENTIREBUFFER); // Flag.

	if (FAILED(hr))
		throw std::runtime_error("failed Lock");

	memcpy((void *)bufAddr, soundFile.getData(), (size_t)bufLen);
	pDsb->Unlock(
		bufAddr,	// Address of lock start.
		bufLen,		// Size of lock.
		NULL,		// No wraparound portion.
		0);			// No wraparound size.

	// rewind the sound
	pDsb->SetCurrentPosition(0);
}

namespace factory {

Sound makeSound(SoundFile& file) {
	return Sound(new DirectSoundBuffer(file));
}

}

} /* namespace jukebox */

