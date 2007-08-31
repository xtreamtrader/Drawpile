/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:
   
   Except as contained in this notice, the name(s) of the above copyright
   holders shall not be used in advertising or otherwise to promote the sale,
   use or other dealings in this Software without prior written authorization.
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "socket.h"

#include "../shared/templates.h"
#include "errors.h"

#include <iostream>
#ifndef NDEBUG
	#include <iostream>
	using std::cout;
	using std::endl;
	using std::cerr;
#endif
#include <string> // std::string
#include <cassert>

#ifndef WIN32 // POSIX
	#include <fcntl.h>
	#ifdef HAVE_SNPRINTF
		#include <cstdio>
	#endif
	#include <cerrno>
#endif

/*
const int FullShutdown = SHUT_RDWR;
const int ShutdownWriting = SHUT_WR;
const int ShutdownReading = SHUT_RD;
*/

using namespace socket_error;
using namespace error;

Socket::Socket(const fd_t& nsock)
	: //ReferenceCounted(),
	sock(nsock)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket::Socket(" << nsock << ")" << std::endl;
	#endif
}

Socket::Socket(const fd_t& nsock, const Address& saddr)
	: //ReferenceCounted(),
	sock(nsock),
	addr(saddr)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket(FD: " << nsock << ", address: " << saddr.toString() << ") constructed" << std::endl;
	#endif
}

Socket::Socket(const Socket& socket)
	: ReferenceCounted(socket),
	sock(socket.sock),
	addr(socket.addr)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket(FD: " << sock << ", address: " << addr.toString() << ") copied [" << (*ref_count) << "]" << std::endl;
	#endif
}

Socket::~Socket()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "~Socket(FD: " << sock << ") destructed" << std::endl;
	#endif
	
	if (unique())
		close();
}

fd_t Socket::create()
{
	if (sock != InvalidHandle)
		close();
	
	#ifdef WIN32
	sock = WSASocket(addr.family, SOCK_STREAM, 0, 0, 0, 0 /* WSA_FLAG_OVERLAPPED */);
	#else // POSIX
	sock = socket(addr.family, SOCK_STREAM, IPPROTO_TCP);
	#endif
	
	if (sock == InvalidHandle)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		assert(s_error != FamilyNotSupported);
		assert(s_error != ProtocolNotSupported);
		assert(s_error != ProtocolType);
		//assert(s_error != ESOCKTNOSUPPORT); // ?
		assert(s_error != EINVAL);
		
		#ifndef NDEBUG
		switch (s_error)
		{
		#ifdef WIN32
		case InProgress:
			break;
		case SubsystemDown:
			cerr << "[Socket] Network sub-system failure" << endl;
			break;
		#endif
		case DescriptorLimit:
			cerr << "[Socket] Socket limit reached" << endl;
			break;
		case OutOfBuffers:
			cerr << "[Socket] out of buffers" << endl;
			break;
		default:
			cerr << "[Socket] Unknown error in create() - " << s_error << endl;
			assert(s_error);
			break;
		}
		#endif
	}
	
	return sock;
}

fd_t Socket::fd() const
{
	return sock;
}

fd_t Socket::fd(fd_t nsock)
{
	if (sock != InvalidHandle) close();
	return sock = nsock;
}

void Socket::close()
{
	#if defined(HAVE_XPWSA)
	::DisconnectEx(sock, 0, TF_REUSE_SOCKET, 0);
	#elif defined(WIN32)
	::closesocket(sock);
	#else // POSIX
	::close(sock);
	#endif
	
	sock = InvalidHandle;
}

Socket Socket::accept()
{
	assert(sock != InvalidHandle);
	
	// temporary address struct
	Address sa;
	
	socklen_t addrlen = sa.size();
	
	#ifdef WIN32
	fd_t n_fd = ::WSAAccept(sock, &sa.addr, &addrlen, 0, 0);
	#else // POSIX
	fd_t n_fd = ::accept(sock, &sa.addr, &addrlen);
	#endif
	
	if (n_fd == InvalidHandle)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != BadDescriptor);
		assert(s_error != EINVAL);
		assert(s_error != Fault);
		assert(s_error != NotSocket);
		assert(s_error != OperationNotSupported);
		
		#ifdef EPROTO
		assert(s_error != EPROTO);
		#endif
		
		#ifdef LINUX
		// no idea what these are
		assert(s_error != ConnectionTimedOut); // Timed out
		assert(s_error != ERESTARTSYS); // ?
		#endif
		
		#ifndef NDEBUG
		switch (s_error)
		{
		case Interrupted: // interrupted
		case WouldBlock: // would block
			break;
		case DescriptorLimit:
			cerr << "[Socket] Process FD limit reached" << endl;
			break;
		case OutOfBuffers:
			cerr << "[Socket] Out of network buffers" << endl;
			break;
		case ConnectionAborted:
			cerr << "[Socket] Incoming connection aborted" << endl;
			break;
		case OutOfMemory:
			cerr << "[Socket] Out of memory" << endl;
			break;
		case InsufficientPermissions:
			cerr << "[Socket] Firewall blocked incoming connection" << endl;
			break;
		#ifndef WIN32 // POSIX
		case SystemDescriptorLimit:
			cerr << "[Socket] System FD limit reached" << endl;
			break;
		#endif
		default:
			cerr << "[Socket] Unknown error in accept() - " << s_error << endl;
			assert(s_error);
			break;
		}
		#endif
	}
	
	return Socket(n_fd, sa);
}

bool Socket::block(const bool x)
{
	#ifndef NDEBUG
	cout << "[Socket] Blocking for socket #" << sock << ": " << (x?"Enabled":"Disabled") << endl;
	#endif // NDEBUG
	
	assert(sock != InvalidHandle);
	
	#ifdef WIN32
	uint32_t arg = (x ? 1 : 0);
	return (WSAIoctl(sock, FIONBIO, &arg, sizeof(arg), 0, 0, 0, 0, 0) == 0);
	#else // POSIX
	assert(x == false);
	return fcntl(sock, F_SETFL, O_NONBLOCK) == Error ? false : true;
	#endif
}

bool Socket::reuse_port(const bool x)
{
	#ifndef NDEBUG
	cout << "[Socket] Reuse port of socket #" << sock << ": " << (x?"Enabled":"Disabled") << endl;
	#endif
	
	assert(sock != InvalidHandle);
	
	#ifndef SO_REUSEPORT
	// Windows (for example) does not have it
	return (x==true);
	#else // POSIX
	int val = (x ? 1 : 0);
	
	const int r = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char*)&val, sizeof(int));
	
	if (r == Error)
	{
		s_error = errno;
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != BadDescriptor);
		assert(s_error != NotSocket);
		assert(s_error != ProtocolOption);
		assert(s_error != Fault);
		
		#ifndef NDEBUG
		cerr << "[Socket] Unknown error in reuse_port() - " << s_error << endl;
		#endif
	}
	
	return (r == 0);
	#endif
}

bool Socket::reuse_addr(const bool x)
{
	#ifndef NDEBUG
	cout << "[Socket] Reuse address of socket #" << sock << ": " << (x?"Enabled":"Disabled") << endl;
	#endif
	
	assert(sock != InvalidHandle);
	
	#ifndef SO_REUSEADDR
	// If the system doesn't have it
	return (x==true);
	#else // POSIX
	int val = (x ? 1 : 0);
	
	const int r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(int));
	
	if (r == Error)
	{
		s_error = errno;
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != BadDescriptor);
		assert(s_error != NotSocket);
		assert(s_error != ProtocolOption);
		assert(s_error != Fault);
		
		#ifndef NDEBUG
		cerr << "[Socket] Unknown error in reuse_addr() - " << s_error << endl;
		#endif
	}
	
	return (r == 0);
	#endif
}

bool Socket::linger(const bool x, const ushort delay)
{
	#ifndef NDEBUG
	cout << "[Socket] Linger for socket #" << sock << ": " << (x?"Enabled":"Disabled") << endl;
	#endif
	
	::linger lval;
	lval.l_onoff = (x ? 1 : 0);
	lval.l_linger = delay;
	
	const int r = setsockopt(sock, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&lval), sizeof(lval));
	
	if (r == Error)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		assert(s_error != BadDescriptor);
		assert(s_error != NotSocket);
		assert(s_error != ProtocolOption);
		assert(s_error != Fault);
		
		#ifndef NDEBUG
		cerr << "[Socket] Unknown error in linger() - " << s_error << endl;
		#endif
	}
	
	return (r == 0);
}

int Socket::bindTo(const std::string& address, const ushort _port)
{
	#if !defined(NDEBUG)
	cout << "[Socket] Binding to address " << address << ":" << _port << endl;
	#endif
	
	assert(sock != InvalidHandle);
	
	Address naddr = Address::fromString(address);
	
	naddr.port(_port);
	
	return bindTo(naddr);
}

int Socket::bindTo(const Address& naddr)
{
	addr = naddr;
	
	#ifndef NDEBUG
	assert(addr.family != Network::Family::None);
	#endif
	
	const int r = bind(sock, &addr.addr, addr.size());
	
	if (r == Error)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != BadDescriptor);
		assert(s_error != EINVAL);
		assert(s_error != NotSocket);
		assert(s_error != OperationNotSupported);
		assert(s_error != FamilyNotSupported);
		assert(s_error != Connected);
		
		#ifndef NDEBUG
		cerr << "[Socket] Failed to bind address and port." << endl;
		
		switch (s_error)
		{
		case AddressInUse:
			cerr << "[Socket] Address already in use" << endl;
			break;
		case AddressNotAvailable:
			cerr << "[Socket] Address not available" << endl;
			break;
		case OutOfBuffers:
			cerr << "[Socket] Out of network buffers" << endl;
			break;
		case EACCES:
			cerr << "[Socket] Can't bind to super-user ports" << endl;
			break;
		default:
			cerr << "[Socket] Unknown error in bindTo() [error: " << s_error << "]" << endl;
			break;
		}
		#endif
	}
	
	return r;
}

int Socket::connect(const Address& rhost)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Connecting to " << rhost.toString() << endl;
	#endif
	
	assert(sock != InvalidHandle);
	
	addr = rhost;
	
	#ifdef WIN32
	const int r = WSAConnect(sock, &addr.addr, addr.size(), 0, 0, 0, 0);
	#else // POSIX
	const int r = ::connect(sock, &addr.addr, addr.size());
	#endif
	
	if (r == Error)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		// programming errors
		assert(s_error != BadDescriptor);
		assert(s_error != Fault);
		assert(s_error != NotSocket);
		assert(s_error != Connected);
		assert(s_error != AddressInUse);
		assert(s_error != FamilyNotSupported);
		assert(s_error != Already);
		
		switch (s_error)
		{
		case InProgress:
			break;
		case EACCES:
		#ifdef EPERM
		case EPERM:
			break;
		#endif
		case ConnectionRefused:
		case ConnectionTimedOut:
		case Unreachable:
			break;
		default:
			break;
		}
	}
	
	return r;
}

int Socket::listen()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Listening" << endl;
	#endif
	
	assert(sock != InvalidHandle);
	
	const int r = ::listen(sock, 4);
	
	if (r == Error)
	{
		#ifdef WIN32
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		assert(s_error != BadDescriptor);
		assert(s_error != NotSocket);
		assert(s_error != OperationNotSupported);
		
		#ifndef NDEBUG
		cerr << "[Socket] Failed to open listening port. [error: " << s_error << "]" << endl;
		#endif // NDEBUG
	}
	
	return r;
}

int Socket::send(char* buffer, const size_t len)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Sending " << len << " bytes" << endl;
	#endif
	
	assert(buffer != 0);
	assert(len > 0);
	assert(sock != InvalidHandle);
	
	#ifdef WIN32
	WSABUF wbuf;
	wbuf.buf = buffer;
	wbuf.len = len;
	u_long sb;
	const int r = ::WSASend(sock, &wbuf, 1, &sb, 0, 0, 0);
	#else // POSIX
	const int r = ::send(sock, buffer, len, NoSignal);
	#endif
	
	if (r == Error)
	{
		#ifdef WIN32
		s_error = ::WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != Fault);
		assert(s_error != EINVAL);
		assert(s_error != BadDescriptor);
		assert(s_error != NotConnected);
		assert(s_error != NotSocket);
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#else
		assert(s_error != OperationNotSupported);
		#endif
		
		switch (s_error)
		{
		case WouldBlock:
		case Interrupted:
			break;
		case ConnectionBroken:
		case ConnectionReset:
			break;
		case OutOfMemory:
		case OutOfBuffers:
			break;
		#ifdef WIN32
		case SubsystemDown: // Network sub-system failure
		case WSAENETRESET: // Keep-alive reset
		case ConnectionAborted: // Connection timed-out
		case WSA_IO_PENDING: // Operation will be completed later
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
			break;
		#endif
		default:
			#ifndef NDEBUG
			cerr << "[Socket] Unknown error in send() - " << s_error << endl;
			#endif // NDEBUG
			assert(s_error);
			break;
		}
		
		#ifdef WIN32
		return r;
		#endif
	}
	
	#ifdef WIN32
	return sb;
	#else
	return r;
	#endif
}

int Socket::recv(char* buffer, const size_t len)
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	cout << "[Socket] Receiving at most " << len << " bytes" << endl;
	#endif
	
	assert(sock != InvalidHandle);
	assert(buffer != 0);
	assert(len > 0);
	
	#ifdef WIN32
	WSABUF wbuf;
	wbuf.buf = buffer;
	wbuf.len = len;
	u_long flags=0;
	u_long rb;
	const int r = ::WSARecv(sock, &wbuf, 1, &rb, &flags, 0, 0);
	#else // POSIX
	const int r = ::recv(sock, buffer, len, 0);
	#endif
	
	if (r == Error)
	{
		#ifdef WIN32
		s_error = ::WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		#ifdef WIN32
		assert(s_error != WSANOTINITIALISED);
		#endif
		
		assert(s_error != BadDescriptor);
		assert(s_error != Fault);
		assert(s_error != EINVAL);
		assert(s_error != NotConnected);
		assert(s_error != NotSocket);
		
		switch (s_error)
		{
		case WouldBlock:
		case Interrupted:
			break;
		case OutOfBuffers: // Out of buffers
		case ConnectionReset:
		case ConnectionRefused:
			break;
		case OutOfMemory:
			break;
		#ifdef WIN32
		case Disconnected:
		case Shutdown:
		case SubsystemDown: // Network sub-system failure
		case NetworkReset: // Keep-alive reset
		case ConnectionAborted: // Connection timed-out
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
		case WSA_IO_PENDING: // Operation will be completed later
			break;
		#endif
		default:
			#ifndef NDEBUG
			cerr << "[Socket] Unknown error in recv() - " << s_error << endl;
			#endif // NDEBUG
			assert(s_error);
			break;
		}
		
		#ifdef WIN32
		return r;
		#endif
	}
	
	#ifdef WIN32
	return rb;
	#else
	return r;
	#endif
}

std::string Socket::address() const
{
	return addr.toString();
}

ushort Socket::port() const
{
	return addr.port();
}

int Socket::shutdown(int how)
{
	return ::shutdown(sock, how);
}

int Socket::getError() const
{
	return s_error;
}

Address& Socket::getAddr()
{
	return addr;
}

const Address& Socket::getConstAddr() const
{
	return addr;
}

#ifdef SOCKET_OPS
bool Socket::operator== (const Socket& tsock) const
{
	return (sock == tsock.sock);
}

Socket& Socket::operator= (Socket& tsock)
{
	if (sock != InvalidHandle)
		close(sock);
	sock = tsock.sock;
	return *this;
}
#endif