/*
    Copyright 2019 Roberto Panerai Velloso.
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

#include "AlsaState.h"

namespace jukebox {

AlsaState::AlsaState(AlsaHandle &alsa) : alsa(alsa), volume(100) {
}

AlsaState::AlsaState(AlsaState &state) : alsa(state.alsa), volume(state.volume) {
}

int AlsaState::getVolume() const {
	return volume;
}

void AlsaState::setVolume(int vol) {
	volume = vol;
}
} /* namespace jukebox */
