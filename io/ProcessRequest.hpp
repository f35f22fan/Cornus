#pragma once

#include <QThread>

#include "../ByteArray.hpp"
#include "../err.hpp"
#include "../decl.hxx"
#include "decl.hxx"

namespace cornus::io {

class ProcessRequest : public QThread
{
	Q_OBJECT
public:
	ProcessRequest(args_data *ad): args_(ad) {}
	
	void run() override;
	
Q_SIGNALS:
	void resultReady(const QString &s);
	
private:
	args_data *args_ = nullptr;
};

}
