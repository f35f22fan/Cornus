#pragma once

#include <QFileSystemModel>
#include <QLineEdit>

#include "../err.hpp"
#include "../decl.hxx"
#include "../io/decl.hxx"
#include "decl.hxx"

namespace cornus::gui {

class Location: public QLineEdit {
public:
	Location(cornus::App *app);
	virtual ~Location();
	
	void SetIncludeHiddenDirs(const bool flag);
	void SetLocation(const QString &s);
	
protected:
	virtual void focusInEvent(QFocusEvent *evt) override;
	
private:
	
	void HitEnter();
	void Setup();
	
	cornus::App *app_ = nullptr;
	QFileSystemModel *fs_model_ = nullptr;
};

}
