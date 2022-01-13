#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../io/decl.hxx"
#include "../err.hpp"
#include "../input.hxx"

#include <QAbstractTableModel>
#include <QMouseEvent>
#include <QPoint>
#include <QTableView>
#include <QUrl>

namespace cornus::gui {
struct OpenWith {
	QString full_path;
	QString mime;
	QVector<DesktopFile*> show_vec;
	QVector<DesktopFile*> hide_vec;
	
	void Clear();
};

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
	void ExecuteDrop(QVector<io::File *> *files_vec, io::File *to_dir,
		Qt::DropAction drop_action, Qt::DropActions possible_actions);
	TableHeader* header() const { return header_; }
	int IsOnFileName(const QPoint &pos);
	TableModel* model() const { return model_; }
	const QPoint& drop_coord() const { return drop_coord_; }
	io::File* GetFileAt_NoLock(const QPoint &pos, const Clone clone, int *ret_file_index = nullptr);
	void GetSelectedArchives(QVector<QString> &urls);
	/// returns row index & cloned file if on file name, otherwise -1
	int GetFileUnderMouse(const QPoint &local_pos, io::File **ret_cloned_file, QString *full_path = nullptr);
	void GetSelectedFileNames(QVector<QString> &names,
		const StringCase str_case = StringCase::AsIs);
	int GetRowHeight() const;
	i32 GetVisibleRowsCount() const;
	i32 IsOnFileName_NoLock(const QPoint &local_pos, io::File **ret_file = nullptr);
	bool mouse_down() const { return mouse_down_; }
	i32 mouse_over_file_icon_index() const { return mouse_over_file_icon_; }
	i32 mouse_over_file_name_index() const { return mouse_over_file_name_; }
	const OpenWith& open_with() const { return open_with_; }
	void ProcessAction(const QString &action);
	bool ReloadOpenWith();
	void RemoveThumbnailsFromSelectedFiles();
	void ScrollToFile(int file_index);
	void SelectByLowerCase(QVector<QString> filenames, const NamesAreLowerCase are_lower);
	ShiftSelect* shift_select() { return &shift_select_; }
	void ShowVisibleColumnOptions(QPoint pos);
	void SyncWith(const cornus::Clipboard &cl, QVector<int> &indices);
	gui::Tab* tab() const { return tab_; }
	void UpdateColumnSizes() { SetCustomResizePolicy(); }
	QStyleOptionViewItem view_options() const { return viewOptions(); }
	
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
	
	void HandleKeySelect(const VDirection vdir);
	void HandleKeyShiftSelect(const VDirection vdir);
	void HandleMouseRightClickSelection(const QPoint &pos, QVector<int> &indices);
	void HiliteFileUnderMouse();
	i32 IsOnFileIcon_NoLock(const QPoint &local_pos, io::File **ret_file = nullptr);
	void LaunchFromOpenWithMenu();
	void SetCustomResizePolicy();
	void ShowRightClickMenu(const QPoint &global_pos, const QPoint &local_pos);
	void SortingChanged(int logical, Qt::SortOrder order);
	void StartDragOperation();
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
	QVector<int> indices_;
	OpenWith open_with_ = {};
	TableHeader *header_ = nullptr;
	QMenu *undo_delete_menu_ = nullptr;
	
	input::TriggerOn trigger_on_ = input::TriggerOn::FileName;
};

} // cornus::gui::
