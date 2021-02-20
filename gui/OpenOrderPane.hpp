#pragma once

#include <QDialog>
#include <QDialogButtonBox>

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"

QT_BEGIN_NAMESPACE
class QPushButton;
QT_END_NAMESPACE

namespace cornus::gui {

enum class Direction: u8 {
	Up,
	Down
};

class OpenOrderPane: public QDialog {
public:
	OpenOrderPane(App *app);
	virtual ~OpenOrderPane();
	
	OpenOrderModel* model() const { return model_; }
	OpenOrderTable* table() const { return table_; }
	
protected:
	void TableSelectionChanged();

private:
	NO_ASSIGN_COPY_MOVE(OpenOrderPane);
	void ButtonClicked(QAbstractButton *button);
	void CreateGui();
	void MoveItem(const Direction d);
	void QueryData();
	void Save();
	bool WasOrderModified() const;
	
	App *app_ = nullptr;
	
	QDialogButtonBox *button_box_ = nullptr;
	
	OpenOrderTable *table_ = nullptr;
	OpenOrderModel *model_ = nullptr;
	QVector<DesktopFile*> vec_;
	QString mime_;
	QPushButton *up_btn_ = nullptr, *down_btn_ = nullptr;
};
}
