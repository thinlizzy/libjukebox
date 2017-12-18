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

#include "Mixer/Mixer.h"

namespace jukebox {

namespace factory {
	extern MixerImpl &makeMixerImpl();
}

Mixer::Mixer() :
	impl(factory::makeMixerImpl()) {
}

int Mixer::getVolume() {
	auto vol = impl.getVolume();
	vol = std::min(vol, 100);
	vol = std::max(vol, 0);
	return vol;
}

void Mixer::setVolume(int vol) {
	vol = std::min(vol, 100);
	vol = std::max(vol, 0);
	impl.setVolume(vol);
}

} /* namespace jukebox */
