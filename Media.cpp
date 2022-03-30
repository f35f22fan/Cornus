#include "Media.hpp"

#include "App.hpp"
#include "ByteArray.hpp"
#include "io/io.hh"
#include "io/SaveFile.hpp"
#include "prefs.hh"

#include <cstdlib> /// srand, rand
#include <time.h>

namespace cornus {

void Media::AddNTS(const QVector<QString> &names, const media::Field field)
{
	switch (field)
	{
	case media::Field::Actors: {
		data_.actors.insert(data_.actors.size(), names);
		break;
	}
	case media::Field::Directors: {
		data_.directors.insert(data_.directors.size(), names);
		break;
	}
	case media::Field::Writers: {
		data_.writers.insert(data_.writers.size(), names);
		break;
	}
	case media::Field::Genres: {
		data_.genres.insert(data_.genres.size(), names);
		break;
	}
	case media::Field::Subgenres: {
		data_.subgenres.insert(data_.subgenres.size(), names);
		break;
	}
	case media::Field::Countries: {
		data_.countries.insert(data_.countries.size(), names);
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
	const i4 count = ba.next_u2();
	if (f == media::Field::Actors || f == media::Field::Directors
		|| f == media::Field::Writers)
	{
		for (int i = 0; i < count; i++) {
			const auto id = ba.next_i4();
			QVector vec = GetNTS(f, id);
			if (vec.size() > 0)
				cb->addItem(vec[0], id);
		}
	} else if (f == media::Field::Genres || f == media::Field::Subgenres
		|| f == media::Field::Countries) {
		for (int i = 0; i < count; i++) {
			const auto id = ba.next_i2();
			QVector vec = GetNTS(f, id);
			if (vec.size() > 0)
				cb->addItem(vec[0], id);
		}
	} else if (f == media::Field::Rip) {
		for (int i = 0; i < count; i++) {
			const auto id = ba.next_i2();
			QString s = data_.rips.value(id);
			if (!s.isEmpty())
				cb->addItem(s, id);
		}
	} else if (f == media::Field::VideoCodec) {
		for (int i = 0; i < count; i++) {
			const auto id = ba.next_i2();
			QString s = data_.video_codecs.value(id);
			if (!s.isEmpty())
				cb->addItem(s, id);
		}
	} else {
		mtl_trace();
	}
}

void Media::Clear()
{
	data_.actors.clear();
	data_.directors.clear();
	data_.writers.clear();
	data_.genres.clear();
	data_.subgenres.clear();
	data_.countries.clear();
}

void Media::FillInNTS(QComboBox *cb, const media::Field category, const Fill option)
{
	if (category == media::Field::Actors)
	{
		if (option == Fill::AddNoneOption) {
			cb->addItem(QObject::tr("(Actor)"), -1);
		}
		auto it = data_.actors.constBegin();
		while (it != data_.actors.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Directors) {
		if (option == Fill::AddNoneOption) {
			cb->addItem(QObject::tr("(Director)"), -1);
		}
		auto it = data_.directors.constBegin();
		while (it != data_.directors.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Writers) {
		if (option == Fill::AddNoneOption) {
			cb->addItem(QObject::tr("(Writer)"), -1);
		}
		auto it = data_.writers.constBegin();
		while (it != data_.writers.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Genres) {
		if (option == Fill::AddNoneOption) {
			cb->addItem(QObject::tr("(Genre)"), -1);
		}
		auto it = data_.genres.constBegin();
		while (it != data_.genres.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Subgenres) {
		if (option == Fill::AddNoneOption) {
			cb->addItem(QObject::tr("(Subgenre)"), -1);
		}
		auto it = data_.subgenres.constBegin();
		while (it != data_.subgenres.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Countries) {
		if (option == Fill::AddNoneOption) {
			cb->addItem(QObject::tr("(Country)"), -1);
		}
		auto it = data_.countries.constBegin();
		while (it != data_.countries.constEnd()) {
			cb->addItem(it.value()[0], it.key());
			it++;
		}
	} else if (category == media::Field::Rip) {
		if (option == Fill::AddNoneOption) {
			cb->addItem(QObject::tr("(Rip)"), -1);
		}
		auto it = data_.rips.constBegin();
		while (it != data_.rips.constEnd()) {
			cb->addItem(it.value(), it.key());
			it++;
		}
	} else if (category == media::Field::VideoCodec) {
		if (option == Fill::AddNoneOption) {
			cb->addItem(QObject::tr("(Video Codec)"), -1);
		}
		auto it = data_.video_codecs.constBegin();
		while (it != data_.video_codecs.constEnd()) {
			cb->addItem(it.value(), it.key());
			it++;
		}
	} else {
		mtl_trace("Other category");
	}
	
	cb->model()->sort(0, Qt::AscendingOrder);
}

i4 Media::GetMagicNumber()
{
	const bool locked = TryLock();
	i4 ret = data_.magic_number;
	if (locked)
		Unlock();
	return ret;
}

QVector<QString>
Media::GetNTS(const media::Field f, const i8 ID)
{
	if (ID == -1) {
		mtl_trace();
		return QVector<QString>();
	}
	
	switch (f) {
	case media::Field::Actors: {
		return data_.actors.value(i4(ID));
	}
	case media::Field::Directors: {
		return data_.directors.value(i4(ID));
	}
	case media::Field::Writers: {
		return data_.writers.value(i4(ID));
	}
	case media::Field::Genres: {
		return data_.genres.value(i2(ID));
	}
	case media::Field::Subgenres: {
		return data_.subgenres.value(i2(ID));
	}
	case media::Field::Countries: {
		return data_.countries.value(i2(ID));
	}
	default: {
		mtl_trace();
		return QVector<QString>();
	}
	}
}

void Media::NewMagicNumber()
{
	while (data_.magic_number == -1) {
		srand (time(NULL));
		data_.magic_number = rand();
	}
}

void Media::Reload()
{
	io::ReadParams rp = {};
	rp.can_rely = CanRelyOnStatxSize::Yes;
	ByteArray ba;
	MTL_CHECK_VOID(io::ReadFile(prefs::GetMediaFilePath(), ba, rp));
	{
		auto g = guard();
		ReloadDatabaseNTS(ba, data());
	}
}

void Media::ReloadDatabaseNTS(ByteArray &ba, media::Data &data)
{
	Clear();
	
	if (data.rips.isEmpty()) {
		using media::Rip;
		data.rips.insert((i2)Rip::CAMRip, QLatin1String("CAMRip"));
		data.rips.insert((i2)Rip::TS, QLatin1String("TS"));
		data.rips.insert((i2)Rip::TC, QLatin1String("TC"));
		data.rips.insert((i2)Rip::SuperTS, QLatin1String("SuperTS"));
		data.rips.insert((i2)Rip::WP, QLatin1String("WP"));
		data.rips.insert((i2)Rip::SCR, QLatin1String("SCR"));
		data.rips.insert((i2)Rip::DVDScr, QLatin1String("DVDScr"));
		data.rips.insert((i2)Rip::VHSRip, QLatin1String("VHSRip"));
		data.rips.insert((i2)Rip::TVRip, QLatin1String("TVRip"));
		data.rips.insert((i2)Rip::SATRip, QLatin1String("SATRip"));
		data.rips.insert((i2)Rip::IPTVRip, QLatin1String("IPTVRip"));
		data.rips.insert((i2)Rip::DVB, QLatin1String("DVB"));
		data.rips.insert((i2)Rip::HDTV, QLatin1String("HDTV"));
		data.rips.insert((i2)Rip::HDTVRip, QLatin1String("HDTVRip"));
		data.rips.insert((i2)Rip::WEBRip, QLatin1String("WEBRip"));
		data.rips.insert((i2)Rip::WEB_DL, QLatin1String("WEB_DL"));
		data.rips.insert((i2)Rip::WEB_DLRip, QLatin1String("WEB_DLRip"));
		data.rips.insert((i2)Rip::DVD5, QLatin1String("DVD5"));
		data.rips.insert((i2)Rip::DVD9, QLatin1String("DVD9"));
		data.rips.insert((i2)Rip::DVDRip,QLatin1String( "DVDRip"));
		data.rips.insert((i2)Rip::HDRip, QLatin1String("HDRip"));
		data.rips.insert((i2)Rip::BDRip, QLatin1String("BDRip"));
		data.rips.insert((i2)Rip::Hybrid, QLatin1String("Hybrid"));
		data.rips.insert((i2)Rip::HDDVDRip, QLatin1String("HDDVDRip"));
		data.rips.insert((i2)Rip::UHD_BDRip, QLatin1String("UHD_BDRip"));
		data.rips.insert((i2)Rip::BDRemux, QLatin1String("BDRemux"));
		data.rips.insert((i2)Rip::HDDVDRemux, QLatin1String("HDDVDRemux"));
		data.rips.insert((i2)Rip::Blu_Ray, QLatin1String("BluRay"));
		data.rips.insert((i2)Rip::HDDVD, QLatin1String("HDDVD"));
		data.rips.insert((i2)Rip::UHD_Blu_Ray, QLatin1String("UHD_Blu_Ray"));
		data.rips.insert((i2)Rip::UHD_BDRemux, QLatin1String("UHD_BDRemux"));
		
		using media::VideoCodec;
		data.video_codecs.insert((i2)VideoCodec::AV1, QLatin1String("AV1"));
		data.video_codecs.insert((i2)VideoCodec::VP8, QLatin1String("VP8"));
		data.video_codecs.insert((i2)VideoCodec::VP9, QLatin1String("VP9"));
		data.video_codecs.insert((i2)VideoCodec::H263, QLatin1String("H263"));
		data.video_codecs.insert((i2)VideoCodec::H264, QLatin1String("H264"));
		data.video_codecs.insert((i2)VideoCodec::H265, QLatin1String("HEVC"));
		data.video_codecs.insert((i2)VideoCodec::H266, QLatin1String("H266"));
		data.video_codecs.insert((i2)VideoCodec::DivX, QLatin1String("DivX"));
		data.video_codecs.insert((i2)VideoCodec::Xvid, QLatin1String("Xvid"));
		data.video_codecs.insert((i2)VideoCodec::Other, QLatin1String("Other"));
	}
	
	if (ba.size() >= sizeof(data.magic_number)) {
		data.magic_number = ba.next_i4();
	}
	
	while (ba.has_more())
	{
		media::Field field = (media::Field)ba.next_u1();
		const i4 count = ba.next_i4();
		HashI4V *hi32 = nullptr;
		HashI2V *hi16 = nullptr;
		
		if (field == media::Field::Actors)
			hi32 = &data.actors;
		else if (field == media::Field::Directors)
			hi32 = &data.directors;
		else if (field == media::Field::Writers)
			hi32 = &data.writers;
		else if (field == media::Field::Genres)
			hi16 = &data.genres;
		else if (field == media::Field::Subgenres)
			hi16 = &data.subgenres;
		else if (field == media::Field::Countries)
			hi16 = &data.countries;
		else {
			mtl_trace();
			continue;
		}
		
		for (i4 i = 0; i < count; i++)
		{
			const i2 names_count = ba.next_u1();
			QVector<QString> v;
			for (i2 i = 0; i < names_count; i++)
			{
				v.append(ba.next_string());
			}
			
			if (hi32 != nullptr) {
				hi32->insert(hi32->size(), v);
			} else if (hi16 != nullptr) {
				hi16->insert(hi16->size(), v);
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
		auto g = guard();
		changed_by_myself_ = true;
		
		if (data_.magic_number == -1) {
			NewMagicNumber();
			mtl_info("New magic number: %d", data_.magic_number);
		}
		ba.add_i4(data_.magic_number);
		
		if (data_.actors.size() > 0)
		{
			ba.add_u1((u1)media::Field::Actors);
			ba.add_i4(data_.actors.size());
			auto it = data_.actors.constBegin();
			while (it != data_.actors.constEnd())
			{
				WriteAny(ba, it.value());
				it++;
			}
		}
		
		if (data_.directors.size() > 0)
		{
			ba.add_u1((u1)media::Field::Directors);
			ba.add_i4(data_.directors.size());
			auto it = data_.directors.constBegin();
			while (it != data_.directors.constEnd())
			{
				WriteAny(ba, it.value());
				it++;
			}
		}
		
		if (data_.writers.size() > 0)
		{
			ba.add_u1((u1)media::Field::Writers);
			ba.add_i4(data_.writers.size());
			auto it = data_.writers.constBegin();
			while (it != data_.writers.constEnd())
			{
				WriteAny(ba, it.value());
				it++;
			}
		}
		
		if (data_.genres.size() > 0)
		{
			ba.add_u1((u1)media::Field::Genres);
			ba.add_i4(data_.genres.size());
			auto it = data_.genres.constBegin();
			while (it != data_.genres.constEnd())
			{
				WriteAny(ba, it.value());
				it++;
			}
		}
		
		if (data_.subgenres.size() > 0)
		{
			ba.add_u1((u1)media::Field::Subgenres);
			ba.add_i4(data_.subgenres.size());
			auto it = data_.subgenres.constBegin();
			while (it != data_.subgenres.constEnd())
			{
				WriteAny(ba, it.value());
				it++;
			}
		}
		
		if (data_.countries.size() > 0)
		{
			ba.add_u1((u1)media::Field::Countries);
			ba.add_i4(data_.countries.size());
			auto it = data_.countries.constBegin();
			while (it != data_.countries.constEnd())
			{
				WriteAny(ba, it.value());
				it++;
			}
		}
	}
	
	io::SaveFile save_file(prefs::GetMediaFilePath());
	if (!io::WriteToFile(save_file.GetPathToWorkWith(), ba.data(), ba.size()))
	{
		mtl_trace();
		save_file.CommitCancelled();
		return;
	}
	
	save_file.Commit();
}

i8 Media::SetNTS(const media::Field f, const i8 ID,
	const QVector<QString> &names, i8 *existing_id, const media::Action action,
	const media::Check check)
{
	const bool append = (action == media::Action::Append);
	if (!append && (ID == -1))
	{
		mtl_trace();
		return -1;
	}
	
	i8 id = -1;
	
	switch (f) {
	case media::Field::Actors: {
		if (check == media::Check::Exists) {
			auto it = data_.actors.constBegin();
			while (it != data_.actors.constEnd()) {
				if (it.value() == names) {
					if (existing_id != nullptr)
						*existing_id = it.key();
					return -1;
				}
				it++;
			}
		}
		id = data_.actors.size();
		data_.actors.insert(append ? i4(id) : i4(ID), names); break;
	}
	case media::Field::Directors: {
		if (check == media::Check::Exists) {
			auto it = data_.directors.constBegin();
			while (it != data_.directors.constEnd()) {
				if (it.value() == names) {
					if (existing_id != nullptr)
						*existing_id = it.key();
					return -1;
				}
				it++;
			}
		}
		id = data_.directors.size();
		data_.directors.insert(append ? i4(id) : i4(ID), names); break;
	}
	case media::Field::Writers: {
		auto it = data_.writers.constBegin();
		while (it != data_.writers.constEnd()) {
			if (it.value() == names) {
				if (existing_id != nullptr)
					*existing_id = it.key();
				return -1;
			}
			it++;
		}
		id = data_.writers.size();
		data_.writers.insert(append ? i4(id) : i4(ID), names); break;
	}
	case media::Field::Genres: {
		auto it = data_.genres.constBegin();
		while (it != data_.genres.constEnd()) {
			if (it.value() == names) {
				if (existing_id != nullptr)
					*existing_id = it.key();
				return -1;
			}
			it++;
		}
		id = data_.genres.size();
		data_.genres.insert(append ? i2(id) : i2(ID), names); break;
	}
	case media::Field::Subgenres: {
		auto it = data_.subgenres.constBegin();
		while (it != data_.subgenres.constEnd()) {
			if (it.value() == names) {
				if (existing_id != nullptr)
					*existing_id = it.key();
				return -1;
			}
			it++;
		}
		id = data_.subgenres.size();
		data_.subgenres.insert(append ? i2(id) : i2(ID), names); break;
	}
	case media::Field::Countries: {
		auto it = data_.countries.constBegin();
		while (it != data_.countries.constEnd()) {
			if (it.value() == names) {
				if (existing_id != nullptr)
					*existing_id = it.key();
				return -1;
			}
			it++;
		}
		id = data_.countries.size();
		data_.countries.insert(append ? i2(id) : i2(ID), names); break;
	}
	default: {
		mtl_trace();
		return -1;
	}
	}
	
	return append ? id : ID;
}

void Media::WriteAny(ByteArray &ba, const QVector<QString> &names)
{
	MTL_CHECK_VOID(names.size() < 255);
	const u1 count = names.size();
	ba.add_u1(count);
	
	for (i2 i = 0; i < count; i++) {
		ba.add_string(names[i]);
	}
}

void Media::WriteTo(ByteArray &ba, QComboBox *cb, const media::Field f)
{
	const int count = cb->count();
	if (count == 0)
		return;
	ba.add_u1((u1)f);
	ba.add_u2(count);
	
	if (f == media::Field::Actors || f == media::Field::Directors
		|| f == media::Field::Writers)
	{
		for (int i = 0; i < count; i++) {
			QVariant v = cb->itemData(i);
			const i4 n = v.toLongLong();
			ba.add_i4(n);
		}
	} else if (f == media::Field::Genres || f == media::Field::Subgenres
		|| f == media::Field::Countries) {
		for (int i = 0; i < count; i++) {
			QVariant v = cb->itemData(i);
			const i2 n = v.toInt();
			ba.add_i2(n);
		}
	} else if (f == media::Field::Rip) {
		for (int i = 0; i < count; i++) {
			QVariant v = cb->itemData(i);
			const i2 n = v.toInt();
			ba.add_i2(n);
		}
	} else if (f == media::Field::VideoCodec) {
		for (int i = 0; i < count; i++) {
			QVariant v = cb->itemData(i);
			const i2 n = v.toInt();
			ba.add_i2(n);
		}
	} else {
		mtl_trace();
	}
}

void PrintVec(const i8 id, const QVector<QString> &names)
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
		auto it = data_.actors.constBegin();
		while (it != data_.actors.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
	
	{
		mtl_info("Directors:");
		auto it = data_.directors.constBegin();
		while (it != data_.directors.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
	
	{
		mtl_info("Writers:");
		auto it = data_.writers.constBegin();
		while (it != data_.writers.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
	
	{
		mtl_info("Genres:");
		auto it = data_.genres.constBegin();
		while (it != data_.genres.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
	
	{
		mtl_info("Subgenres:");
		auto it = data_.subgenres.constBegin();
		while (it != data_.subgenres.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
	
	{
		mtl_info("Countries:");
		auto it = data_.countries.constBegin();
		while (it != data_.countries.constEnd()) {
			PrintVec(it.key(), it.value());
			it++;
		}
	}
}
}
