#    This file is part of snapcast
#   Copyright (C) 2014-2017  Johannes Pohl
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

.PHONY: all check-env flac ogg tremor

all: check-env flac ogg tremor

check-env:
#	if [ ! -d "flac" ]; then \
#		$(error flac directory missing); \
#	fi; \
#	if [ ! -d ogg ]; then \
#		$(error ogg directory missing); \
#	fi; \
#	if [ ! -d tremor ]; then \
#		$(error tremor directory missing); \
#	fi
ifndef NDK_DIR
	$(error android NDK_DIR is not set)
endif

flac:
	@cd flac; \
	export CC=$(NDK_DIR)/bin/arm-linux-androideabi-clang; \
	export CXX=$(NDK_DIR)/bin/arm-linux-androideabi-clang++; \
	export CPPFLAGS="-U_ARM_ASSEM_ -I$(NDK_DIR)/include"; \
	./autogen.sh; \
	./configure --host=arm --disable-ogg --prefix=$(NDK_DIR); \
	make; \
	make install; \
	make clean;

ogg:
	@cd ogg; \
	export CC=$(NDK_DIR)/bin/arm-linux-androideabi-clang; \
	export CXX=$(NDK_DIR)/bin/arm-linux-androideabi-clang++; \
	export CPPFLAGS="-U_ARM_ASSEM_ -I$(NDK_DIR)/include"; \
	./autogen.sh; \
	./configure --host=arm --prefix=$(NDK_DIR); \
	make; \
	make install; \
	make clean;

tremor:
	@cd tremor; \
	export CC=$(NDK_DIR)/bin/arm-linux-androideabi-clang; \
	export CXX=$(NDK_DIR)/bin/arm-linux-androideabi-clang++; \
	export CPPFLAGS="-U_ARM_ASSEM_ -I$(NDK_DIR)/include"; \
	./autogen.sh; \
	./configure --host=arm --prefix=$(NDK_DIR) --with-ogg=$(NDK_DIR); \
	make; \
	make install; \
	make clean; \
	rm -rf .deps/; \
	rm Makefile; \
	rm Makefile.in; \
	rm Version_script; \
	rm aclocal.m4; \
	rm -rf autom4te.cache/; \
	rm compile; \
	rm config.guess; \
	rm config.h; \
	rm config.h.in; \
	rm config.log; \
	rm config.status; \
	rm config.sub; \
	rm configure; \
	rm depcomp; \
	rm install-sh; \
	rm libtool; \
	rm ltmain.sh; \
	rm missing; \
	rm stamp-h1; \
	rm vorbisidec.pc;


