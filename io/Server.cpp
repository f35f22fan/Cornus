#include "Server.hpp"

#include "../str.hxx"
#include "../gui/TasksWin.hpp"
#include "io.hh"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>

namespace cornus::io {

Server::Server()
{
	tasks_win_ = new gui::TasksWin();
}

Server::~Server() {}

void Server::CopyToClipboard(const QString &s)
{
	QMimeData *mime = new QMimeData();
	
	auto list = s.splitRef('\n');
	QList<QUrl> urls;
	for (auto &next: list) {
		urls.append(QUrl::fromLocalFile(next.toString()));
	}
	
	mime->setUrls(urls);
	QApplication::clipboard()->setMimeData(mime);
}

void Server::CutToClipboard(const QString &s)
{
	QMimeData *mime = new QMimeData();
	
	auto list = s.splitRef('\n');
	QList<QUrl> urls;
	for (auto &next: list) {
		urls.append(QUrl::fromLocalFile(next.toString()));
	}
	
	mime->setUrls(urls);
	
	QByteArray ba;
	char c = '1';
	ba.append(c);
	mime->setData(str::KdeCutMime, ba);
	
	QApplication::clipboard()->setMimeData(mime);
}

} // namespace


