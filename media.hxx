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
using HashI2S = QMap<i2, QString>;
using HashI2V = QMap<i2, QVector<QString>>;
using HashI4V = QMap<i4, QVector<QString>>;

namespace media {
static const QString XAttrName = QStringLiteral("user.CornusMas.m");
static const QString XAttrLastWatched = QStringLiteral("user.CornusMas.lw");
static const QString XAttrThumbnail = QStringLiteral("user.CornusMas.thmb");

struct Data {
	HashI4V actors;
	HashI4V directors;
	HashI4V writers;
	
	HashI2V genres;
	HashI2V subgenres;
	HashI2V countries;
	
	HashI2S rips;
	HashI2S video_codecs;
	i4 magic_number = -1;
};

struct MediaPreview {
	QVector<i4> actors;
	QVector<i4> directors;
	QVector<i4> writers;
	
	QVector<i2> genres;
	QVector<i2> subgenres;
	QVector<i2> countries;
	
	QVector<i2> rips;
	QVector<i2> video_codecs;
	i4 video_w = -1;
	i4 video_h = -1;
	i4 magic_number = -1;
	f4 fps = -1;
	i2 year_started = -1;
	i2 year_end = -1;
	i2 bit_depth = -1;
	
	i1 month_started = -1;
	i1 day_started = -1;
};

enum class Check: i1 {
	Exists,
	None,
};

enum class Action: i1 {
	Insert,
	Append
};

enum class Rip: i2 {
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

enum class VideoCodec: i2 {
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

enum class Field: u1 {
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
	Comments,
	
	// Added 2022.03.30:
	MonthStarted,
	DayStarted,
};

}} // cornus::media::
