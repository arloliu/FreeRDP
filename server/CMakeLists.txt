# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP Servers
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Servers

if(NOT WIN32)

	# Build Test Server
	add_subdirectory(test)

	# Build X11 Server
	find_suggested_package(X11)
	if(WITH_X11)
		add_subdirectory(X11)
	endif()
else()
	add_subdirectory(Windows)
endif()

# Build Server Channels library
add_subdirectory(channels)
