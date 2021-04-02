#include "Media.hpp"

#include "App.hpp"
#include "ByteArray.hpp"
#include "io/io.hh"
#include "prefs.hh"

#include <cstdlib> /// srand, rand
#include <time.h>

namespace cornus {
namespace media {
void Reload(App *app)
{
	ByteArray ba;
	
	if (io::ReadFile(prefs::GetMediaFilePath(), ba) != io::Err::Ok) {
		mtl_trace();
		return;
	}
	
	Media *media = app->media();
	{
		auto guard = media->guard();
		media->ReloadDatabaseNTS(ba);
	}
}
} /// media::

void Media::AddNTS(const QVector<QString> &names, const media::Field field)
{
	switch (field)
	{
	case media::Field::Actors: {
		actors_.insert(actors_.size(), names);
		break;
	}
	case  media::Field::Directors: {
		directors_.insert(directors_.size(), names);
		break;
	}
	case media::Field::Writers: {
		writers_.insert(writers_.size(), names);
		break;
	}
	case media::Field::Genres: {
		genres_.insert(genres_.size(), names);
		break;
	}
	case media::Field::Subgenres: {
		subgenres_.insert(subgenres_.size(), names);
		break;
	}
	case media::Field::Countries: {
		countries_.insert(countries_.size(), names);
		break;
	}
	default: {
		mtl_trace();
	}
	}
}

void Media::ApplyTo(QComboBox *cb, ByteArray &ba, const media::Field f)
{
	auto g = guard();
	const i32 count = ba.next_i32();
	if (f == media::Field::Actors || f == media::Field::Directors
		|| f == media::Field::Writers)
	{
		for (int i = 0; i < count; i++) {
			const u32 id = ba.next_u32();
			QVector vec = GetNTS(f, id);
			if (vec.size() > 0)
				cb->addItem(vec[0], id);
		}
	} else if (f == media::Field::Genres || f == media::Field::Subgenres
		|| f == media::Field::Countries) {
		for (int i = 0; i < count; i++) {
			const u16 id = ba.next_u16();
			QVector vec = GetNTS(f, id);
			if (vec.size() > 0)
				cb->addItem(vec[0], id);
		}
	} else {
		mtl_trace();
	}
}

void Media::Clear()
{
	genres_.clear();
	subgenres_.clear();
	countries_.clear();
	directors_.clear();
	actors_.clear();
	writers_.clear();
	loaded_ = false;
}

void Media::FillInNTS(QComboBox *cb, const media::Field category)
{
	if (category == media::Field::Actors)
	{
		auto it = actors_.constBegin();
		while (it != actors_.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Directors) {
		auto it = directors_.constBegin();
		while (it != directors_.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Writers) {
		auto it = writers_.constBegin();
		while (it != writers_.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Genres) {
		auto it = genres_.constBegin();
		while (it != genres_.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Subgenres) {
		auto it = subgenres_.constBegin();
		while (it != subgenres_.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Countries) {
		auto it = countries_.constBegin();
		while (it != countries_.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else {
		mtl_trace("Other category");
	}
	
	cb->model()->sort(0, Qt::AscendingOrder);
}

i32 Media::GetMagicNumber()
{
	const bool locked = TryLock();
	i32 ret = magic_number_;
	if (locked)
		Unlock();
	return ret;
}

QVector<QString>
Media::GetNTS(const media::Field f, const i64 ID)
{
	if (ID == -1) {
		mtl_trace();
		return QVector<QString>();
	}
	
	switch (f) {
	case media::Field::Actors: {
		return actors_.value(u32(ID));
	}
	case media::Field::Directors: {
		return directors_.value(u32(ID));
	}
	case media::Field::Writers: {
		return writers_.value(u32(ID));
	}
	case media::Field::Genres: {
		return genres_.value(u16(ID));
	}
	case media::Field::Subgenres: {
		return subgenres_.value(u16(ID));
	}
	case media::Field::Countries: {
		return countries_.value(u16(ID));
	}
	default: {
		mtl_trace();
		return QVector<QString>();
	}
	}
}

i64 Media::SetNTS(const media::Field f, const i64 ID,
	const QVector<QString> &names, const media::Action action)
{
	const bool append = (action == media::Action::Append);
	if (!append && (ID == -1))
	{
		mtl_trace();
		return -1;
	}
	
	i64 id = -1;
	
	switch (f) {
	case media::Field::Actors: {
		id = actors_.size();
		actors_.insert(append ? u32(id) : u32(ID), names); break;
	}
	case media::Field::Directors: {
		id = directors_.size();
		directors_.insert(append ? u32(id) : u32(ID), names); break;
	}
	case media::Field::Writers: {
		id = writers_.size();
		writers_.insert(append ? i32(id) : u32(ID), names); break;
	}
	case media::Field::Genres: {
		id = genres_.size();
		genres_.insert(append ? i16(id) : u16(ID), names); break;
	}
	case media::Field::Subgenres: {
		id = subgenres_.size();
		subgenres_.insert(append ? u16(id) : u16(ID), names); break;
	}
	case media::Field::Countries: {
		id = countries_.size();
		countries_.insert(append ? u16(id) : u16(ID), names); break;
	}
	default: {
		mtl_trace();
		return -1;
	}
	}
	
	return append ? id : ID;
}

void Media::NewMagicNumber()
{
	srand (time(NULL));
	magic_number_ = rand();
}

void Media::ReloadDatabaseNTS(ByteArray &ba)
{
	Clear();
	loaded_ = true;
	if (ba.size() >= sizeof(magic_number_)) {
		magic_number_ = ba.next_i32();
	}
	
	while (ba.has_more())
	{
		media::Field field = (media::Field)ba.next_u8();
		const i32 count = ba.next_i32();
		HashU32 *hu32 = nullptr;
		HashU16 *hu16 = nullptr;
		
		if (field == media::Field::Actors)
			hu32 = &actors_;
		else if (field == media::Field::Directors)
			hu32 = &directors_;
		else if (field == media::Field::Writers)
			hu32 = &writers_;
		else if (field == media::Field::Genres)
			hu16 = &genres_;
		else if (field == media::Field::Subgenres)
			hu16 = &subgenres_;
		else if (field == media::Field::Countries)
			hu16 = &countries_;
		else {
			mtl_trace();
			continue;
		}
		
		for (i32 i = 0; i < count; i++)
		{
			const i16 names_count = ba.next_u8();
			QVector<QString> v;
			for (i16 i = 0; i < names_count; i++)
			{
				v.append(ba.next_string());
			}
			
			if (hu32 != nullptr) {
				hu32->insert(hu32->size(), v);
			} else if (hu16 != nullptr) {
				hu16->insert(hu16->size(), v);
			} else {
				mtl_trace();
			}
		}
	}
}

void Media::Save()
{
	ByteArray ba;
	{
		MutexGuard g = guard();
		changed_by_myself_ = true;
		
		if (magic_number_ == -1) {
			NewMagicNumber();
			mtl_info("New magic number: %d", magic_number_);
		}
		ba.add_i32(magic_number_);
		
		if (actors_.size() > 0)
		{
			ba.add_u8((u8)media::Field::Actors);
			ba.add_i32(actors_.size());
			foreach (auto &v, actors_) {
				WriteAny(ba, v);
			}
		}
		
		if (directors_.size() > 0)
		{
			ba.add_u8((u8)media::Field::Directors);
			ba.add_i32(directors_.size());
			foreach (auto &v, directors_) {
				WriteAny(ba, v);
			}
		}
		
		if (writers_.size() > 0)
		{
			ba.add_u8((u8)media::Field::Writers);
			ba.add_i32(writers_.size());
			foreach (auto &v, writers_) {
				WriteAny(ba, v);
			}
		}
		
		if (genres_.size() > 0)
		{
			ba.add_u8((u8)media::Field::Genres);
			ba.add_i32(genres_.size());
			foreach (auto &v, genres_) {
				WriteAny(ba, v);
			}
		}
		
		if (subgenres_.size() > 0)
		{
			ba.add_u8((u8)media::Field::Subgenres);
			ba.add_i32(subgenres_.size());
			foreach (auto &v, subgenres_) {
				WriteAny(ba, v);
			}
		}
		
		if (countries_.size() > 0)
		{
			ba.add_u8((u8)media::Field::Countries);
			ba.add_i32(countries_.size());
			foreach (auto &v, countries_) {
				WriteAny(ba, v);
			}
		}
	}
	
	QString full_path = prefs::GetMediaFilePath();
	io::WriteToFile(full_path, ba.data(), ba.size());
}

void Media::WriteAny(ByteArray &ba, const QVector<QString> &names)
{
	CHECK_TRUE_VOID((names.size() < 255));
	const u8 count = names.size();
	ba.add_u8(count);
	
	for (i16 i = 0; i < count; i++) {
		ba.add_string(names[i]);
	}
}

void Media::WriteTo(ByteArray &ba, QComboBox *cb, const media::Field f)
{
	const int count = cb->count();
	if (count == 0)
		return;
	ba.add_u8((u8)f);
	ba.add_i32(count);
	
	if (f == media::Field::Actors || f == media::Field::Directors
		|| f == media::Field::Writers)
	{
		for (int i = 0; i < count; i++) {
			QVariant v = cb->itemData(i);
			u32 n = v.toLongLong();
			ba.add_u32(n);
///			mtl_info("added: %u", n);
		}
	} else if (f == media::Field::Genres || f == media::Field::Subgenres
		|| f == media::Field::Countries) {
		for (int i = 0; i < count; i++) {
			QVariant v = cb->itemData(i);
			u16 n = v.toInt();
			ba.add_u16(n);
///			mtl_info("added: %u", n);
		}
	} else {
		mtl_trace();
	}
}

void PrintVec(const i64 id, const QVector<QString> &names)
{
	mtl_info("ID: %ld", id);
	for (auto &next: names) {
		mtl_printq2("Name: ", next);
	}
}

void Media::Print()
{
	{
		mtl_info("Actors:");
		auto it = actors_.constBegin();
		while (it != actors_.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
	
	{
		mtl_info("Directors:");
		auto it = directors_.constBegin();
		while (it != directors_.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
	
	{
		mtl_info("Writers:");
		auto it = writers_.constBegin();
		while (it != writers_.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
	
	{
		mtl_info("Genres:");
		auto it = genres_.constBegin();
		while (it != genres_.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
	
	{
		mtl_info("Subgenres:");
		auto it = subgenres_.constBegin();
		while (it != subgenres_.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
	
	{
		mtl_info("Countries:");
		auto it = countries_.constBegin();
		while (it != countries_.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
}
}
