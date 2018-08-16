/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// this code is common to all unix variants (linux/bsd/osx)

#include "DebuggerCoreUNIX.h"
#include "edb.h"

#include <QtGlobal>
#include <QProcess>
#include <QStringList>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef Q_OS_LINUX
#include <linux/version.h>
// being very conservative for now, technically this could be
// as low as 2.6.22
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
#define USE_SIGTIMEDWAIT
#endif
#endif

namespace DebuggerCorePlugin {
namespace {

struct Exception {
    qlonglong         value;
	const char *const name;
} Exceptions[] = {
#ifdef SIGABRT
    { SIGABRT, "SIGABRT" },
#endif
#ifdef SIGALRM
    { SIGALRM, "SIGALRM" },
#endif
#ifdef SIGVTALRM
    { SIGVTALRM, "SIGVTALRM" },
#endif
#ifdef SIGPROF
    { SIGPROF, "SIGPROF" },
#endif
#ifdef SIGBUS
    { SIGBUS, "SIGBUS" },
#endif
#ifdef SIGCHLD
    { SIGCHLD, "SIGCHLD" },
#endif
#ifdef SIGCONT
    { SIGCONT, "SIGCONT" },
#endif
#ifdef SIGFPE
    { SIGFPE, "SIGFPE" },
#endif
#ifdef SIGHUP
    { SIGHUP, "SIGHUP" },
#endif
#ifdef SIGILL
    { SIGILL, "SIGILL" },
#endif
#ifdef SIGINT
    { SIGINT, "SIGINT" },
#endif
#ifdef SIGKILL
    { SIGKILL, "SIGKILL" },
#endif
#ifdef SIGPIPE
    { SIGPIPE, "SIGPIPE" },
#endif
#ifdef SIGQUIT
    { SIGQUIT, "SIGQUIT" },
#endif
#ifdef SIGSEGV
    { SIGSEGV, "SIGSEGV" },
#endif
#ifdef SIGSTOP
    { SIGSTOP, "SIGSTOP" },
#endif
#ifdef SIGTERM
    { SIGTERM, "SIGTERM" },
#endif
#ifdef SIGTSTP
    { SIGTSTP, "SIGTSTP" },
#endif
#ifdef SIGTTIN
    { SIGTTIN, "SIGTTIN" },
#endif
#ifdef SIGTTOU
    { SIGTTOU, "SIGTTOU" },
#endif
#ifdef SIGUSR1
    { SIGUSR1, "SIGUSR1" },
#endif
#ifdef SIGUSR2
    { SIGUSR2, "SIGUSR2" },
#endif
#ifdef SIGPOLL
    { SIGPOLL, "SIGPOLL" },
#endif
#ifdef SIGSYS
    { SIGSYS, "SIGSYS" },
#endif
#ifdef SIGTRAP
    { SIGTRAP, "SIGTRAP" },
#endif
#ifdef SIGURG
    { SIGURG, "SIGURG" },
#endif
#ifdef SIGXCPU
    { SIGXCPU, "SIGXCPU" },
#endif
#ifdef SIGXFSZ
    { SIGXFSZ, "SIGXFSZ" },
#endif
#ifdef SIGRTMIN
    { SIGRTMIN, "SIGRTMIN" },
#endif
#ifdef SIGRTMAX
    { SIGRTMAX, "SIGRTMAX" },
#endif
#ifdef SIGIO
    { SIGIO, "SIGIO" },
#endif
#ifdef SIGSTKFLT
    { SIGSTKFLT, "SIGSTKFLT" },
#endif
#ifdef SIGWINCH
    { SIGWINCH, "SIGWINCH" },
#endif
};

}

#if !defined(USE_SIGTIMEDWAIT)
namespace {

int selfpipe[2];
struct sigaction old_action;

//------------------------------------------------------------------------------
// Name: sigchld_handler
// Desc:
//------------------------------------------------------------------------------
void sigchld_handler(int sig, siginfo_t *info, void *p) {

	if(sig == SIGCHLD) {
		native::write(selfpipe[1], " ", sizeof(char));
	}

    // load as volatile
    volatile struct sigaction *vsa = &old_action;

    if (old_action.sa_flags & SA_SIGINFO) {
        void (*oldAction)(int, siginfo_t *, void *) = vsa->sa_sigaction;

        if (oldAction) {
            oldAction(sig, info, p);
		}
    } else {
        void (*oldAction)(int) = vsa->sa_handler;

        if (oldAction && oldAction != SIG_IGN) {
            oldAction(sig);
		}
    }
}

}
#endif

//------------------------------------------------------------------------------
// Name: read
// Desc: read, but handles being interrupted
//------------------------------------------------------------------------------
ssize_t native::read(int fd, void *buf, size_t count) {
	ssize_t ret;
	do {
		ret = ::read(fd, buf, count);
	} while(ret == -1 && errno == EINTR);
	return ret;
}

//------------------------------------------------------------------------------
// Name: write
// Desc: write, but handles being interrupted
//------------------------------------------------------------------------------
ssize_t native::write(int fd, const void *buf, size_t count) {
	ssize_t ret;
	do {
		ret = ::write(fd, buf, count);
	} while(ret == -1 && errno == EINTR);
	return ret;
}

//------------------------------------------------------------------------------
// Name: select
// Desc: select but handles being interrupted
//------------------------------------------------------------------------------
int native::select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	int ret;
	do {
		ret = ::select(nfds, readfds, writefds, exceptfds, timeout);
	} while(ret == -1 && errno == EINTR);
	return ret;
}

//------------------------------------------------------------------------------
// Name: waitpid
// Desc: waitpid, but handles being interrupted
//------------------------------------------------------------------------------
pid_t native::waitpid(pid_t pid, int *status, int options) {
	pid_t ret;
	do {
		ret = ::waitpid(pid, status, options);
	} while(ret == -1 && errno == EINTR);
	return ret;
}

//------------------------------------------------------------------------------
// Name: select_ex
// Desc: similar to select but has the timeout specified as an unsigned quantity of msecs
// Note: msecs == 0 means wait forever.
//------------------------------------------------------------------------------
int native::select_ex(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, quint64 msecs) {
	if(msecs != 0) {
		struct timeval tv;

		tv.tv_sec  = (msecs / 1000);
		tv.tv_usec = (msecs % 1000) * 1000;

		return native::select(nfds, readfds, writefds, exceptfds, &tv);
	} else {
		return native::select(nfds, readfds, writefds, exceptfds, nullptr);
	}
}

//------------------------------------------------------------------------------
// Name: wait_for_sigchld
// Desc:
//------------------------------------------------------------------------------
bool native::wait_for_sigchld(int msecs) {
#if !defined(USE_SIGTIMEDWAIT)
	fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(selfpipe[0], &rfds);

	if(native::select_ex(selfpipe[0] + 1, &rfds, 0, 0, msecs) == 0) {
		return true;
	}

	char ch;
	if(native::read(selfpipe[0], &ch, sizeof(char)) == -1) {
		return true;
	}

	return false;
#else
	sigset_t mask;
	siginfo_t info;
	struct timespec ts;
    ts.tv_sec  = (msecs / 1000);
    ts.tv_nsec = (msecs % 1000) * 1000000;


	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	return sigtimedwait(&mask, &info, &ts) == SIGCHLD;
#endif
}

//------------------------------------------------------------------------------
// Name: DebuggerCoreUNIX
// Desc:
//------------------------------------------------------------------------------
DebuggerCoreUNIX::DebuggerCoreUNIX() {
#if !defined(USE_SIGTIMEDWAIT)

	// HACK(eteran): so, the first time we create a QProcess, it will hook SIGCHLD
	//               unfortunately, in Qt5 it doesn't seem to call our handler
	//               so we do this to force it to hook BEFORE we do, letting us
	//               get the first crack at the signal, then we call the one that
	//               Qt installed.
	auto p = new QProcess(0);
	p->start("/bin/true");

	// create a pipe and make it non-blocking
	int r = ::pipe(selfpipe);
	Q_UNUSED(r);

	::fcntl(selfpipe[0], F_SETFL, ::fcntl(selfpipe[0], F_GETFL) | O_NONBLOCK);
	::fcntl(selfpipe[1], F_SETFL, ::fcntl(selfpipe[1], F_GETFL) | O_NONBLOCK);

	// setup a signal handler
	struct sigaction new_action = {};


	new_action.sa_sigaction = sigchld_handler;
	new_action.sa_flags     = SA_RESTART | SA_SIGINFO;
	sigemptyset(&new_action.sa_mask);

	sigaction(SIGCHLD, &new_action, &old_action);
#else
	// TODO(eteran): the man pages mention blocking the signal was want to catch
	//               but I'm not sure if it is necessary for this use case...
#endif
}

//------------------------------------------------------------------------------
// Name: execute_process
// Desc: tries to execute the process, returns error string on error
//------------------------------------------------------------------------------
Status DebuggerCoreUNIX::execute_process(const QString &path, const QString &cwd, const QList<QByteArray> &args) {

	QString errorString = "internal error";
	
	// change to the desired working directory
	if(::chdir(qPrintable(cwd)) == 0) {

		// allocate space for all the arguments
		auto argv_pointers = new char *[args.count() + 2];

		char **p = argv_pointers;

		*p = new char[path.length() + 1];
		std::strcpy(*p, qPrintable(path));
		++p;

		for(int i = 0; i < args.count(); ++i) {
			const QByteArray s(args[i]);
			*p = new char[s.length() + 1];
			std::strcpy(*p, s.constData());
			++p;
		}

		*p = nullptr;

		// NOTE: it's a bad idea to use execvp and similar functions searching in
		// $PATH. At least on Linux, if the file is corrupted/unsupported, they
		// instead appear to launch shell
		const int ret = execv(argv_pointers[0], argv_pointers);

		// should be no need to cleanup, the process which allocated all that
		// space no longer exists!
		// if we get here...execv failed!
		if(ret == -1) {
			errorString = tr("execv() failed: %1").arg(strerror(errno));
			p = argv_pointers;
			while(*p) {
				delete [] *p++;
			}
			delete [] argv_pointers;
		}
	}
	
	// frankly, any return is technically an error I think
	// this is only executed from a fork
	return Status(errorString);
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCoreUNIX::exceptionName(qlonglong value) {
    for(Exception e : Exceptions) {
        if(value == e.value) {
            return e.name;
        }
    }
    return QString();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
qlonglong DebuggerCoreUNIX::exceptionValue(const QString &name) {
    for(Exception e : Exceptions) {
        if(name == e.name) {
            return e.value;
        }
    }
    return -1;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QMap<qlonglong, QString> DebuggerCoreUNIX::exceptions() const {

    QMap<qlonglong, QString> exceptions;
    for(Exception e : Exceptions) {
        exceptions[e.value] = e.name;
    }
	return exceptions;
}

}
