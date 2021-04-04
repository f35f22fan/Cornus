#pragma once

#include "err.hpp"

namespace cornus::media {

static const QString XAttrName = QStringLiteral("user.CornusMas.m");

enum class Action: i8 {
	Insert,
	Append
};

enum class Rip: u8 {
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

enum class VideoCodec: u8 {
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
};

}
