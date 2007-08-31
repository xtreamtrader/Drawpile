/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "net.h"

#ifdef WIN32
	#include <winsock2.h>
#endif

#ifndef NDEBUG
	#include <cassert>
	#include <iostream>
#endif

bool startNetSubsystem()
{
	#ifdef WIN32
	WSADATA info;
	const int r = WSAStartup(0x0202, &info);
	if (r != 0)
	{
		#ifndef NDEBUG
		assert(r != WSAEFAULT);
		if (r == WSAVERNOTSUPPORTED)
			std::cerr << "Windows sockets version not supported." << std::endl;
		else if (r == WSASYSNOTREADY)
			std::cerr << "O/S network subsystem not ready, yet." << std::endl;
		else
			std::cerr << "Unknown error in starting WSA." << std::endl;
		#endif
		return false;
	}
	
	// We only care the major version is correct (= 2.x)
	if ((info.wVersion >> 8) != 2)
	{
		#ifndef NDEBUG
		std::cerr << "WSA returned invalid version." << std::endl;
		#endif
		
		stopNetSubsystem();
		return false;
	}
	#endif
	
	return true;
}

void stopNetSubsystem()
{
	#ifdef WIN32
	WSACleanup();
	#endif
}