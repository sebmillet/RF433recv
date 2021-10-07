// RF433recv.h

/*
  Copyright 2021 Sébastien Millet

  `RF433recv' is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  `RF433recv' is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program. If not, see
  <https://www.gnu.org/licenses>.
*/

//#define DEBUG

#ifndef _RF433RECV_H
#define _RF433RECV_H

#ifdef DEBUG

#include "RF433Debug.h"

#else

#define dbg(a)
#define dbgf(...)

#endif

#include <Arduino.h>

#endif // _RF433RECV_H

// vim: ts=4:sw=4:tw=80:et
