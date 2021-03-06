#pragma once

#include "err.hpp"

#include <QMap>

namespace cornus {

/** It must be a QMap (not QHash) because
 "When iterating over a QMap, the items are always sorted by key. With QHash,
 the items are arbitrarily ordered."
 Thus when inserting a new item with its ID as the current QMap size it works.
 Hence only a QMap can have the (implied) ID not change.
 Note: QMap items may never be removed, only their names changed.
*/
using HashI16S = QMap<i16, QString>;
using HashI16V = QMap<i16, QVector<QString>>;
using HashI32V = QMap<i32, QVector<QString>>;

namespace media {
static const QString XAttrName = QStringLiteral("user.CornusMas.m");

struct Data {
	HashI32V actors;
	HashI32V directors;
	HashI32V writers;
	
	HashI16V genres;
	HashI16V subgenres;
	HashI16V countries;
	
	HashI16S rips;
	HashI16S video_codecs;
	i32 magic_number = -1;
};

struct ShortData {
	QVector<i32> actors;
	QVector<i32> directors;
	QVector<i32> writers;
	
	QVector<i16> genres;
	QVector<i16> subgenres;
	QVector<i16> countries;
	
	QVector<i16> rips;
	QVector<i16> video_codecs;
	i16 year = -1;
	i16 year_end = -1;
	i32 magic_number = -1;
};

enum class Check: i8 {
	Exists,
	None,
};

enum class Action: i8 {
	Insert,
	Append
};

enum class Rip: i16 {
	None = 0,
	CAMRip,
	TS,
	TC,
	SuperTS,
	WP,
	SCR,
	DVDScr,
	VHSRip,
	TVRip,
	SATRip,
	IPTVRip,
	DVB,
	HDTV,
	HDTVRip,
	WEBRip,
	WEB_DL,
	WEB_DLRip,
	DVD5,
	DVD9,
	DVDRip,
	HDRip,
	BDRip,
	Hybrid,
	HDDVDRip,
	UHD_BDRip,
	BDRemux,
	HDDVDRemux,
	Blu_Ray,
	HDDVD,
	UHD_Blu_Ray,
	UHD_BDRemux,
};

enum class VideoCodec: i16 {
	AV1,
	VP8,
	VP9,
	H263,
	H264,
	H265,
	H266,
	DivX,
	Xvid,
	Other,
};

enum class Field: u8 {
	None = 0,
	Actors,
	Writers,
	Directors,
	Genres,
	Subgenres,
	Countries,
	YearStarted, // e.g. 2011
	YearEnded, // e.g. 2011-2015
	VideoCodecBitDepth,
	Rip,
	VideoCodec,
	VideoResolution,
	FPS,
};

}} // cornus::media::
