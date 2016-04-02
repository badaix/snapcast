/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#ifndef PCM_ENCODER_H
#define PCM_ENCODER_H
#include "encoder.h"


class PcmEncoder : public Encoder
{
public:
	PcmEncoder(const std::string& codecOptions = "");
	virtual void encode(const msg::PcmChunk* chunk);
	virtual std::string name() const;

protected:
    virtual void initEncoder();

	template<typename T>
	void assign(void* pointer, T val)
	{
		T* p = (T*)pointer;
		*p = val;
	}
};


#endif


