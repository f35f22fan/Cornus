#include "category.hh"

namespace cornus {
namespace category {
void InitAll(QHash<QString, Category> &h)
{
	h.insert(str::TextEditor, Category::TextEditor);
	h.insert(str::Gnome, Category::Gnome);
	h.insert(str::KDE, Category::KDE);
	h.insert(str::Xfce, Category::Xfce);
	h.insert(str::Ubuntu, Category::Ubuntu);
	h.insert(str::Unity, Category::Unity);
	h.insert(str::Photography, Category::Photography);
	h.insert(str::Viewer, Category::Viewer);
	h.insert(str::Player, Category::Player);
	h.insert(str::Audio, Category::Audio);
	h.insert(str::Video, Category::Video);
	h.insert(str::AudioVideo, Category::AudioVideo);
}
} /// category::
} /// cornus::
