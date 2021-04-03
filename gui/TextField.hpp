#pragma once

#include <QLineEdit>

namespace cornus::gui {
class TextField : public QLineEdit
{
	Q_OBJECT
	
public:
	TextField(QWidget *parent = 0);
	~TextField();
	
	void select_all_on_focus(const bool flag) {
		select_all_on_focus_ = flag;
	}
	
protected:
	virtual void focusInEvent(QFocusEvent *e) override;
	
private:
	bool select_all_on_focus_ = true; 
};
}
