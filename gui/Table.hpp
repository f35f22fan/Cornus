#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../io/decl.hxx"
#include "../err.hpp"
#include "BasicTable.hpp"

#include <QAbstractTableModel>
#include <QMouseEvent>
#include <QPoint>
#include <QTableView>

namespace cornus::gui {
struct OpenWith {
	QString full_path;
	QVector<DesktopFile*> add_vec;
	QVector<DesktopFile*> remove_vec;
};

class Table : public BasicTable {
	Q_OBJECT
public:
	Table(TableModel *tm, App *app);
	virtual ~Table();
	
	void ApplyPrefs();
	bool CheckIsOnFileName(io::File *file, const int file_row, const QPoint &pos) const;
	void ClearMouseOver();
	int IsOnFileName(const QPoint &pos);
	TableModel* model() const { return model_; }
	const QPoint& drop_coord() const { return drop_coord_; }
	virtual void dropEvent(QDropEvent *event) override;
	io::File* GetFileAtNTS(const QPoint &pos, const Clone clone, int *ret_file_index = nullptr);
	io::File* GetFileAt(const int row); /// returns cloned file
	void GetSelectedArchives(QVector<QString> &urls);
	/// returns row index & cloned file if on file name, otherwise -1
	int GetFileUnderMouse(const QPoint &local_pos, io::File **ret_cloned_file, QString *full_path = nullptr);
	int GetFirstSelectedFile(io::File **ret_cloned_file);
	void GetSelectedFileNames(QVector<QString> &names,
		const StringCase str_case = StringCase::AsIs);
	QString GetFirstSelectedFileFullPath(QString *ext);
	int GetRowHeight() const;
	int GetSelectedFilesCount(QVector<QString> *extensions = nullptr);
	i32 GetVisibleRowsCount() const;
	bool mouse_down() const { return mouse_down_; }
	i32 mouse_over_file_icon_index() const { return mouse_over_file_icon_; }
	i32 mouse_over_file_name_index() const { return mouse_over_file_name_; }
	const OpenWith& open_with() const { return open_with_; }
	void ProcessAction(const QString &action);
	void ScrollToRow(int row);
	void ScrollToAndSelectRow(const int row, const bool deselect_others);
	void SelectAllFilesNTS(const bool flag, QVector<int> &indices);
	void SelectByLowerCase(QVector<QString> filenames);
	void SelectRowSimple(const int row, const bool deselect_others = false);
	void ShowVisibleColumnOptions(QPoint pos);
	void SyncWith(const cornus::Clipboard &cl, QVector<int> &indices);
	virtual void UpdateColumnSizes() override { SetCustomResizePolicy(); }
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
	virtual void wheelEvent(QWheelEvent *event) override;
	virtual void paintEvent(QPaintEvent *evt) override;
	
	
private:
	NO_ASSIGN_COPY_MOVE(Table);
	
	void ActionCopy(QVector<int> &indices);
	void ActionCut(QVector<int> &indices);
	void ActionPaste();
	void ActionPasteLinks(const LinkType link);
	void AddOpenWithMenuTo(QMenu *main_menu, const QString &full_path);
	bool AnyArchive(const QVector<QString> &extensions) const;
	void ClearDndAnimation(const QPoint &drop_coord);
	bool CreateMimeWithSelectedFiles(const ClipboardAction action,
		QVector<int> &indices, QStringList &list);
	
	QVector<QAction*>
	CreateOpenWithList(const QString &full_path);
	
	void FinishDropOperation(QVector<io::File *> *files_vec, io::File *to_dir,
		Qt::DropAction drop_action, Qt::DropActions possible_actions);
	void HandleKeySelect(const VDirection vdir);
	void HandleKeyShiftSelect(const VDirection vdir);
	void HandleMouseRightClickSelection(const QPoint &pos, QVector<int> &indices);
	void HandleMouseSelectionCtrl(const QPoint &pos, QVector<int> &indices);
	void HandleMouseSelectionShift(const QPoint &pos, QVector<int> &indices);
	void HandleMouseSelectionNoModif(const QPoint &pos, QVector<int> &indices, bool mouse_pressed);
	void HiliteFileUnderMouse();
	i32 IsOnFileIconNTS(const QPoint &local_pos, io::File **ret_file = nullptr);
	i32 IsOnFileNameStringNTS(const QPoint &local_pos, io::File **ret_file = nullptr);
	void LaunchFromOpenWithMenu();
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
	i32 mouse_over_file_icon_ = -1;
	struct ShiftSelect {
		int base_row = -1;
		int head_row = -1;
	} shift_select_ = {};

	QPoint drop_coord_ = {-1, -1};
	QPoint drag_start_pos_ = {-1, -1};
	QPoint mouse_pos_ = {-1, -1};
	QVector<int> indices_;
	OpenWith open_with_ = {};
};

} // cornus::gui::
