

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <errno.h>

#ifndef VC_EXTRALEAN
#   define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN 
#   define WIN32_LEAN_AND_MEAN 
#endif
#include <winsock2.h>

#ifdef __cplusplus
}
#endif

#define SOCKERRNO WSAGetLastError()
#define SOCKERRSTR(ERRNO_IN) convertSocketErrorToString(ERRNO_IN)

#define socket_close(S)		closesocket(S)
#define socket_ioctl(A,B,C)	ioctlsocket(A,B,C)
typedef u_long FAR osiSockIoctl_t;

#define MAXHOSTNAMELEN		75
#define IPPORT_USERRESERVED	5000U

#define SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCK_ENOBUFS WSAENOBUFS
#define SOCK_ECONNRESET WSAECONNRESET
#define SOCK_ETIMEDOUT WSAETIMEDOUT
#define SOCK_EADDRINUSE WSAEADDRINUSE
#define SOCK_ECONNREFUSED WSAECONNREFUSED
#define SOCK_ECONNABORTED WSAECONNABORTED
#define SOCK_EINPROGRESS WSAEINPROGRESS
#define SOCK_EISCONN WSAEISCONN
#define SOCK_EALREADY WSAEALREADY
#define SOCK_EINVAL WSAEINVAL
#define SOCK_EINTR WSAEINTR
#define SOCK_EPIPE EPIPE
#define SOCK_EMFILE WSAEMFILE
#define SOCK_SHUTDOWN WSAESHUTDOWN
#define SOCK_ENOTSOCK WSAENOTSOCK

/*
 *	Under WIN32, FD_SETSIZE is the max. number of sockets,
 *	not the max. fd value that you use in select().
 *
 *	Therefore, it is difficult to detemine if any given
 *	fd can be used with FD_SET(), FD_CLR(), and FD_ISSET().
 */
#define FD_IN_FDSET(FD) (1)

epicsShareFunc unsigned epicsShareAPI wsaMajorVersion ();