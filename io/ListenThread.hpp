#pragma once

#include "../ByteArray.hpp"
#include "../err.hpp"
#include "../decl.hxx"
#include "decl.hxx"

#include <QThread>
#include <pthread.h>

namespace cornus::io {

class ListenThread : public QThread
{
	Q_OBJECT
public:
	ListenThread(io::Daemon *daemon) : daemon_(daemon)
	{}
	
	void run() override;
	
Q_SIGNALS:
	void resultReady(const QString &s);
	
private:
	io::Daemon *daemon_ = nullptr;
};

}
