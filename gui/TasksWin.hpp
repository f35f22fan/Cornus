#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/Task.hpp"

#include <QBoxLayout>
#include <QMainWindow>
#include <QTimer>
#include <QList>
#include <QPixmap>
#include <QSystemTrayIcon>


namespace cornus::gui {

class TasksWin: public QMainWindow {
	Q_OBJECT
public:
	TasksWin(QSystemTrayIcon *tray_icon);
	~TasksWin();
	
	QColor green_color() const {
		return (theme_type_ == ThemeType::Light) ? QColor(0, 100, 0) : QColor(200, 255, 200);
	}
	
	bool isDarkMode() const { return theme_type_ == ThemeType::Dark; }
	QColor hover_bg_color() const { return (isDarkMode()) ? QColor(0, 80, 0) : QColor(150, 255, 150); }
	QColor hover_bg_color_gray(const QColor &c) const;
	QColor opposite_to_bg() const { return isDarkMode() ? QColor(255, 255, 255) : QColor(0, 0, 0); }
	
	virtual QSize sizeHint() const override;
	virtual QSize minimumSizeHint() const override;
	QSize maximumSize() const;
	
	void TaskDone(TaskGui *gui_task, const io::TaskState state);
	
public Q_SLOTS:
	void add(cornus::io::Task *task);
	void UpdateOverallProgress();
	
private:
	NO_ASSIGN_COPY_MOVE(TasksWin);
	
	void CreateGui();
	void DetectThemeType();
	void DrawPercent(cint percent);
	
	QBoxLayout *layout_ = nullptr;
	QWidget *main_widget_ = nullptr;
	QSize screen_sz_ = {};
	int win_w_ = -1;
	QTimer *progress_timer_ = nullptr;
	QList<TaskGui*> tasks_;
	
	QSystemTrayIcon *tray_icon_ = nullptr;
	QPixmap pixmap_;
	QIcon icon_;
	QIcon default_icon_;
	ThemeType theme_type_ = ThemeType::None;
};

}
