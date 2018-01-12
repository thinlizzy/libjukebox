/*
    Copyright 2017 Roberto Panerai Velloso.
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

#include <algorithm>
#include <cstdint>
#include "AlsaHandle.h"
#include "jukebox/Sound/Sound.h"

#ifndef ALSA_DEVICE
#define ALSA_DEVICE "sysdefault"
#endif

#ifndef ALSA_MIN_FRAMES
#define ALSA_MIN_FRAMES 100
#endif

namespace {

constexpr size_t minFrames = ALSA_MIN_FRAMES;

class StatusGuard {
public:
  StatusGuard(std::atomic<bool> &status, bool entry) :
	  status(status),
	  exitStatus(!entry) {

	  status = entry;
  };

  ~StatusGuard() {
	  status = exitStatus;
  }
private:
  std::atomic<bool> &status;
  bool exitStatus;
};

}

namespace jukebox {

void closeAlsaHandle(snd_pcm_t *handle) {
	if (handle != nullptr) {
		snd_pcm_drain(handle);
		snd_pcm_close(handle);
	}
}

// AlsaHandle

AlsaHandle::AlsaHandle(SoundFile &file) :
	SoundImpl(file),
	handlePtr(nullptr, closeAlsaHandle) {

	snd_pcm_t *handle;
	auto res = snd_pcm_open(&handle, ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
	if (res != 0)
		throw std::runtime_error("snd_pcm_open error.");

	handlePtr.reset(handle);
	config();
	prepare();
}

void AlsaHandle::play() {
	playThread = std::thread([this]() {
		StatusGuard statusGuard(playing, true);

		size_t frameSize = (soundFile.getBitsPerSample()/8) * soundFile.getNumChannels();
		size_t numFrames = soundFile.getDataSize() / frameSize;
		std::unique_ptr<char[]> volBuf(new char[minFrames*frameSize]);
		position = 0;

		while (numFrames > 0 && playing) {
			auto frames = std::min(numFrames, minFrames);
			auto bytes = soundFile.read(volBuf.get(), position, frames*frameSize);

			if (soundFile.getBitsPerSample() == 16)
				applyVolume(reinterpret_cast<int16_t *>(volBuf.get()), bytes);
			else
				applyVolume(volBuf.get(), bytes);

			auto n = snd_pcm_writei(handlePtr.get(), volBuf.get(), bytes / frameSize);
			if (n > 0) {
				numFrames -= n;
				position += n * frameSize;
			} else
				throw std::runtime_error("snd_pcm_writei error.");
		}
	});
}

template<typename T>
void AlsaHandle::applyVolume(T *buf, size_t len) {
	std::for_each(buf, buf+(len/sizeof(T)), [this](T &c){
		c = static_cast<T>(static_cast<double>(vol)/100.0*static_cast<double>(c));
	});
}

void AlsaHandle::stop() {
	if (playing)
		playing = false;

	if (playThread.joinable())
		playThread.join();

	snd_pcm_drop(handlePtr.get());
	prepare();
}

AlsaHandle::~AlsaHandle() {
	stop();
};

void AlsaHandle::config() {
  auto res = snd_pcm_set_params(
    handlePtr.get(),
	soundFile.getBitsPerSample() == 16 ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U8,
	SND_PCM_ACCESS_RW_INTERLEAVED,
    soundFile.getNumChannels(),
    soundFile.getSampleRate(),
    1,
    500000);
  if (res != 0)
    throw std::runtime_error("snd_pcm_set_params error.");
}

void AlsaHandle::prepare() {
	auto res = snd_pcm_prepare(handlePtr.get());
	if (res != 0)
		throw std::runtime_error("snd_pcm_prepare error.");

	playing = false;
}

int AlsaHandle::getVolume() {
	return vol;
}

void AlsaHandle::setVolume(int vol) {
	this->vol = vol;
}

namespace factory {

Sound makeSound(SoundFile& file) {
	return Sound(new AlsaHandle(file));
}

}

} /* namespace jukebox */
