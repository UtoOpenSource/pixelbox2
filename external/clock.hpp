/* Clocksource
 * Copyright (C) 2023-2024 UtoECat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

namespace pb {

	/** most accurate clocksource in the system */
	extern class ClockSource {
		double oldtime = 0; 
		float  frame_time = 0;
		void init();

		public:
		/** get most presize time in system. never changes back. Threadsafe?*/
		static double time();
		/** return delta time from last tick() call. Not thread safe*/
		float  delta();
		/** sets time position for delta() function. Not thread safe*/
		float  tick();
		ClockSource();
		~ClockSource(); 

	} __clocksource;
	
}