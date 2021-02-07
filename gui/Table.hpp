#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../io/decl.hxx"
#include "../err.hpp"

#include <QAbstractTableModel>
#include <QMouseEvent>
#include <QPoint>
#include <QTableView>

namespace cornus::gui {

class Table : public QTableView {
	Q_OBJECT
public:
	Table(TableModel *tm, App *app);
	virtual ~Table();
	
	void ApplyPrefs();
	bool CheckIsOnFileName(io::File *file, const int file_row, const QPoint &pos) const;
	int IsOnFileName(const QPoint &pos);
	TableModel* model() const { return model_; }
	const QPoint& drop_coord() const { return drop_coord_; }
	virtual void dropEvent(QDropEvent *event) override;
	io::File* GetFileAtNTS(const QPoint &pos, const bool clone, int *ret_file_index = nullptr);
	io::File* GetFileAt(const int row); /// returns cloned file
	
	/// returns row index & cloned file if on file name, otherwise -1
	int GetFileUnderMouse(const QPoint &local_pos, io::File **ret_cloned_file);
	int GetFirstSelectedFile(io::File **ret_cloned_file);
	QString GetFirstSelectedFileFullPath(QString *ext);
	inline int GetIconSize();
	int GetSelectedFilesCount();
	bool mouse_down() const { return mouse_down_; }
	i32 mouse_over_file_name_index() const { return mouse_over_file_name_; }
	void mouse_over_file_name_index(const i32 n) { mouse_over_file_name_ = n; }
	void ProcessAction(const QString &action);
	void ScrollToRow(int row);
	void ScrollToAndSelectRow(const int row, const bool deselect_others);
	void SelectAllFilesNTS(const bool flag, QVector<int> &indices);
	void SelectRowSimple(const int row, const bool deselect_others = false);
	void ShowVisibleColumnOptions(QPoint pos);
	
public slots:
	bool ScrollToAndSelect(QString full_path);
	
protected:
	virtual void dragEnterEvent(QDragEnterEvent *evt) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *evt) override;
	virtual void dragMoveEvent(QDragMoveEvent *evt) override;
	
	virtual void keyPressEvent(QKeyEvent *evt) override;
	virtual void keyReleaseEvent(QKeyEvent *evt) override;
	virtual void leaveEvent(QEvent *evt) override;
	virtual void mouseDoubleClickEvent(QMouseEvent *evt) override;
	virtual void mouseMoveEvent(QMouseEvent *evt) override;
	virtual void mousePressEvent(QMouseEvent *evt) override;
	virtual void mouseReleaseEvent(QMouseEvent *evt) override;
	
	virtual void paintEvent(QPaintEvent *evt) override;
private:
	NO_ASSIGN_COPY_MOVE(Table);
	
	void ActionCopy();
	void ActionCut();
	void ActionPaste();
	void ClearDndAnimation(const QPoint &drop_coord);
	QMimeData* CreateMimeWithSelectedFiles();
	void FinishDropOperation(QVector<io::File *> *files_vec, io::File *to_dir,
		Qt::DropAction drop_action, Qt::DropActions possible_actions);
	ClipboardAction GetClipboardFiles(const QMimeData &mime, QVector<QString> &file_paths);
	void HandleKeySelect(const bool up);
	void HandleKeyShiftSelect(const bool up);
	void HandleMouseRightClickSelection(const QPoint &pos, QVector<int> &indices);
	void HandleMouseSelectionCtrl(const QPoint &pos, QVector<int> &indices);
	void HandleMouseSelectionShift(const QPoint &pos, QVector<int> &indices);
	void HandleMouseSelectionNoModif(const QPoint &pos, QVector<int> &indices, bool mouse_pressed);
	
	int IsOnFileNameStringNTS(const QPoint &local_pos, io::File **ret_file = nullptr);
	QPair<int, int> ListSelectedFiles(QList<QUrl> &list);
	
	void SelectFileRangeNTS(const int row_start, const int row_end, QVector<int> &indices);
	int SelectNextRow(const int relative_offset, const bool deselect_all_others, QVector<int> &indices); // returns newly selected row
	void SetCustomResizePolicy();
	void ShowRightClickMenu(const QPoint &global_pos, const QPoint &local_pos);
	void SortingChanged(int logical, Qt::SortOrder order);
	void StartDragOperation();
	void UpdateLineHeight();
	
	App *app_ = nullptr;
	TableModel *model_ = nullptr;
	TableDelegate *delegate_ = nullptr;
	bool mouse_down_ = false;
	i32 mouse_over_file_name_ = -1;
	
	struct ShiftSelect {
		int base_row = -1;
		int head_row = -1;
	} shift_select_ = {};

	QPoint drop_coord_ = {-1, -1};
	QPoint drag_start_pos_ = {-1, -1};
	
};

} // cornus::gui::
