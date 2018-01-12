/*
    This file is part of libjukebox.
    libjukebox is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    libjukebox is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with libjukebox.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <cstring>
#include <cmath>

#include "DirectSoundBuffer.h"
#include "jukebox/Sound/Sound.h"

namespace jukebox {

class DSoundDevice {
public:
	static struct IDirectSound &getDevice() {
		static DSoundDevice instance;
		return *(instance.lpds);
	}
private:
	LPDIRECTSOUND lpds;

	DSoundDevice() {
		// create device
		auto hr = DirectSoundCreate(NULL, &lpds, NULL);

		if (FAILED(hr))
			throw std::runtime_error("failed DirectSoundCreate");

		// set cooperative level to priority
		hr = lpds->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);

		if (FAILED(hr))
			throw std::runtime_error("failed SetCooperativeLevel");
	}
};


void ReleaseBuffer(LPDIRECTSOUNDBUFFER pDsb) {
	if (pDsb != nullptr)
		pDsb->Release();
}

DirectSoundBuffer::DirectSoundBuffer(SoundFile &file) :
	SoundImpl(file),
	pDsb(nullptr, ReleaseBuffer) {

	prepare();
}

DirectSoundBuffer::~DirectSoundBuffer() {
	stop();
	if (loadBufferThread.joinable())
		loadBufferThread.join();
}

void DirectSoundBuffer::loop(bool l) {
	looping = l;
}

DWORD DirectSoundBuffer::status() {
	  DWORD st;

	  pDsb->GetStatus(&st);
	  return st;
}

void DirectSoundBuffer::play() {
  if (!playing()) {
    // rewind the sound
    pDsb->SetCurrentPosition(0);
    position = 0;

    DWORD playFlags = looping?DSBPLAY_LOOPING:0;

    if (fillBuffer(0, dsbdesc.dwBufferBytes)) {
    	startThread();
    	playFlags = DSBPLAY_LOOPING;
    }

    auto hr = pDsb->Play(
        0,	// Unused.
        0,	// Priority for voice management.
		playFlags);	// Flags.

    if (FAILED(hr))
      throw std::runtime_error("failed Play");
  }
}

void DirectSoundBuffer::stop() {
	auto hr = pDsb->Stop();
	if (FAILED(hr))
		throw std::runtime_error("failed Stop");
}

bool DirectSoundBuffer::playing() {
	return status() & DSBSTATUS_PLAYING;
}

bool DirectSoundBuffer::fillBuffer(int offset, size_t size) {
	// fill secondary buffer with wav/sound
	LPVOID bufAddr;
	DWORD bufLen;

	auto hr = pDsb->Lock(
	      offset,	// Offset at which to start lock.
	      size,		// Size of lock.
	      &bufAddr,	// Gets address of first part of lock.
	      &bufLen,	// Gets size of first part of lock.
	      NULL,		// Address of wraparound not needed.
	      NULL,		// Size of wraparound not needed.
	      0); // Flag.

	if (FAILED(hr))
		throw std::runtime_error("failed Lock");

	size_t len = soundFile.read((char *)bufAddr, position, bufLen);
	position += len;
	if (len < bufLen)
		memset((char *)bufAddr+len, 0, bufLen-len);

	pDsb->Unlock(
		bufAddr,	// Address of lock start.
		bufLen,		// Size of lock.
		NULL,		// No wraparound portion.
		0);			// No wraparound size.

	return position < soundFile.getDataSize();
}

class GuardHandle {
public:
	GuardHandle(HANDLE handle) : handle(handle) {};
	~GuardHandle() {CloseHandle(handle);};
private:
	HANDLE handle;
};

void DirectSoundBuffer::startThread() {
	LPDIRECTSOUNDNOTIFY notifyIface;
	DSBPOSITIONNOTIFY notifyPos[3];

	auto hr = pDsb->QueryInterface(IID_IDirectSoundNotify, (LPVOID*)&notifyIface);
	if (FAILED(hr))
		throw std::runtime_error("failed QueryInterface");

	notifyPos[0].dwOffset = (dsbdesc.dwBufferBytes / 2) - 1;
	notifyPos[0].hEventNotify = CreateEvent(nullptr, false, false, nullptr);
	notifyPos[1].dwOffset = dsbdesc.dwBufferBytes - 1;
	notifyPos[1].hEventNotify = notifyPos[0].hEventNotify;
	notifyPos[2].dwOffset = DSCBPN_OFFSET_STOP;
	notifyPos[2].hEventNotify = notifyPos[0].hEventNotify;

	hr = notifyIface->SetNotificationPositions(3, notifyPos);
	notifyIface->Release();
	if (FAILED(hr))
		throw std::runtime_error("failed SetNotificationPositions");

	if (loadBufferThread.joinable())
		loadBufferThread.join();

	loadBufferThread = std::thread(
		[this](auto event){
			GuardHandle guardHandle(event);

			size_t offsets[2] = {0, dsbdesc.dwBufferBytes / 2};

			do {

				bool offset = true;
				do {
					offset = !offset;
					WaitForSingleObject(event, INFINITE);
				} while (
					playing() &&
					fillBuffer(offsets[offset], dsbdesc.dwBufferBytes / 2));

				if (playing()) {
					WaitForSingleObject(event, INFINITE);
					position = 0;
					fillBuffer(0, dsbdesc.dwBufferBytes);
				}

			} while (looping && playing());

			if (playing())
				stop();
		},
		notifyPos[0].hEventNotify);
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
			DSBCAPS_CTRLPOSITIONNOTIFY |
			DSBCAPS_GLOBALFOCUS;
	/*
	 * DSBCAPS_CTRL3D				The sound source can be moved in 3D space.
	 * DSBCAPS_CTRLFX				Effects can be added to the buffer.
	 * DSBCAPS_CTRLFREQUENCY		The frequency of the sound can be changed.
	 * DSBCAPS_CTRLPAN				The sound source can be moved from left to right.
	 * DSBCAPS_CTRLPOSITIONNOTIFY	Notification positions can be set on the buffer.
	 * DSBCAPS_CTRLVOLUME			The volume of the sound can be changed.
	 */

	dsbdesc.dwBufferBytes = wfx.nBlockAlign * 1024 * 4 * 2;
	dsbdesc.lpwfxFormat = &wfx;

	LPDIRECTSOUNDBUFFER bufPtr;
	auto hr = DSoundDevice::getDevice().CreateSoundBuffer(&dsbdesc, &bufPtr, NULL);

	if (FAILED(hr))
		throw std::runtime_error("failed CreateSoundBuffer");

	pDsb.reset(bufPtr);
}

/*
 * dB to linear scale and vice-versa:
 * https://stackoverflow.com/questions/36072054/get-the-inverse-of-a-function-millibels-to-percentage-percentage-to-millibels
 * */

int DirectSoundBuffer::getVolume() {
	LONG vol;
	pDsb->GetVolume(&vol);

	double exponent = ((((double)vol) / 1000.0) + 10);
	double numerator = 100.0 * (std::pow(2, exponent) - 1);
	return numerator / 1023.0;
}

void DirectSoundBuffer::setVolume(int vol) {
	double attenuation = 1.0 / 1024.0 + ((double)vol) / 100.0 * 1023.0 / 1024.0;
	double db = 10 * std::log10(attenuation) / std::log10(2);
	pDsb->SetVolume(db * 100);
}

namespace factory {

Sound makeSound(SoundFile& file) {
	return Sound(new DirectSoundBuffer(file));
}

}


} /* namespace jukebox */
