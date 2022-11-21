#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../io/decl.hxx"
#include "../err.hpp"
#include "../input.hxx"

#include <QAbstractTableModel>
#include <QMouseEvent>
#include <QPoint>
#include <QSet>
#include <QTableView>
#include <QUrl>

namespace cornus::gui {

class Table : public QTableView {
	Q_OBJECT
public:
	Table(TableModel *tm, App *app, Tab *tab);
	virtual ~Table();
	
	input::TriggerOn trigger_policy() const { return trigger_on_; }
	
	App* app() const { return app_; }
	void ApplyPrefs();
	void AutoScroll(const VDirection d);
	bool CheckIsOnFileName(io::File *file, const int file_row, const QPoint &pos) const;
	void ClearMouseOver();
	const QPoint& drop_coord() const { return drop_coord_; }
	i32 GetFileAt_NoLock(const QPoint &local_pos, const PickBy pb, io::File **ret_file = nullptr);
	io::File* GetFileAt_NoLock(const QPoint &pos, const Clone clone, int *ret_file_index = nullptr);
	int GetVisibleFileIndex();
	TableHeader* header() const { return header_; }
	TableModel* model() const { return model_; }
	int GetRowHeight() const;
	i32 GetVisibleRowsCount() const;
	bool mouse_down() const { return mouse_down_; }
	i32 mouse_over_file_icon_index() const { return mouse_over_file_icon_; }
	i32 mouse_over_file_name_index() const { return mouse_over_file_name_; }
	void ScrollToFile(int file_index);
	void SelectByLowerCase(QVector<QString> filenames, const NamesAreLowerCase are_lower);
	ShiftSelect* shift_select() { return &shift_select_; }
	void ShowVisibleColumnOptions(QPoint pos);
	void SyncWith(const cornus::Clipboard &cl, QSet<int> &indices);
	gui::Tab* tab() const { return tab_; }
	void UpdateColumnSizes() { SetCustomResizePolicy(); }
	QStyleOptionViewItem view_options() const { return viewOptions(); }
	QScrollBar* scrollbar() const { return verticalScrollBar(); }
public Q_SLOTS:
	bool ScrollToAndSelect(QString full_path);
	
protected:
	virtual void dragEnterEvent(QDragEnterEvent *evt) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *evt) override;
	virtual void dragMoveEvent(QDragMoveEvent *evt) override;
	virtual void dropEvent(QDropEvent *event) override;
	
	virtual void keyPressEvent(QKeyEvent *evt) override;
	virtual void keyReleaseEvent(QKeyEvent *evt) override;
	virtual void leaveEvent(QEvent *evt) override;
	virtual void mouseDoubleClickEvent(QMouseEvent *evt) override;
	virtual void mouseMoveEvent(QMouseEvent *evt) override;
	virtual void mousePressEvent(QMouseEvent *evt) override;
	virtual void mouseReleaseEvent(QMouseEvent *evt) override;
	virtual void wheelEvent(QWheelEvent *event) override;
	virtual void paintEvent(QPaintEvent *evt) override;
	
private:
	NO_ASSIGN_COPY_MOVE(Table);
	
	void ClearDndAnimation(const QPoint &drop_coord);
	void HiliteFileUnderMouse();
	i32 IsOnFileIcon_NoLock(const QPoint &local_pos, io::File **ret_file = nullptr);
	void SetCustomResizePolicy();
	void SortingChanged(int logical, Qt::SortOrder order);
	void UpdateLineHeight();
	
	App *app_ = nullptr;
	TableModel *model_ = nullptr;
	gui::Tab *tab_ = nullptr;
	TableDelegate *delegate_ = nullptr;
	bool mouse_down_ = false;
	i32 mouse_over_file_name_ = -1;
	i32 mouse_over_file_icon_ = -1;
	ShiftSelect shift_select_ = {};
	
	struct DragScroll {
		int by = 1;
	} drag_scroll_ = {};

	QPoint drop_coord_ = {-1, -1};
	QPoint drag_start_pos_ = {-1, -1};
	QPoint mouse_pos_ = {-1, -1};
	TableHeader *header_ = nullptr;
	
	input::TriggerOn trigger_on_ = input::TriggerOn::FileName;
};

} // cornus::gui::
