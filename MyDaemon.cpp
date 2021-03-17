#include "MyDaemon.hpp"

#include "err.hpp"
#include "io/io.hh"

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace cornus {
static int sighupFd[2];
static int sigtermFd[2];

MyDaemon::MyDaemon(QObject *parent): QObject(parent)
{
	if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sighupFd))
		qFatal("Couldn't create HUP socketpair");
	
	if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd))
		qFatal("Couldn't create TERM socketpair");
	snHup = new QSocketNotifier(sighupFd[1], QSocketNotifier::Read, this);
	connect(snHup, SIGNAL(activated(QSocketDescriptor)), this, SLOT(handleSigHup()));
	snTerm = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, this);
	connect(snTerm, SIGNAL(activated(QSocketDescriptor)), this, SLOT(handleSigTerm()));
}

MyDaemon::~MyDaemon() {}

void MyDaemon::hupSignalHandler(int)
{
	char a = 1;
	::write(sighupFd[0], &a, sizeof(a));
}

void MyDaemon::termSignalHandler(int)
{
	char a = 1;
	::write(sigtermFd[0], &a, sizeof(a));
}

void MyDaemon::handleSigTerm()
{
	snTerm->setEnabled(false);
	char tmp;
	::read(sigtermFd[1], &tmp, sizeof(tmp));
	
	// do Qt stuff
	io::socket::SendQuitSignalToServer();
	
	snTerm->setEnabled(true);
}

void MyDaemon::handleSigHup()
{
	snHup->setEnabled(false);
	char tmp;
	::read(sighupFd[1], &tmp, sizeof(tmp));
	
	// do Qt stuff
	io::socket::SendQuitSignalToServer();
	
	snHup->setEnabled(true);
}

}


