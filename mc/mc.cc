#include "mc.hh"

// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// This sample application demonstrates how to use the matroska parser
// library, which allows clients to handle a matroska format file.

#include "mkvreader.hpp"
#include "mkvparser.hpp"

#include "../err.hpp"
#include "../types.hxx"

namespace cornus::mc {

static const wchar_t* utf8towcs(const char* str)
{
	if (str == NULL)
		return NULL;

	//TODO: this probably requires that the locale be
	//configured somehow:

	const size_t size = mbstowcs(NULL, str, 0);

	if (size == 0)
		return NULL;

	wchar_t* const val = new wchar_t[size+1];

	mbstowcs(val, str, size);
	val[size] = L'\0';

	return val;
}

QString ReadMkvTitle(QStringView full_path, bool *ok)
{
	if (ok)
		*ok = false;

	using namespace mkvparser;
	MkvReader reader;
	auto path_ba = full_path.toLocal8Bit();
	if (reader.Open(path_ba.data()))
	{
		printf("\n Filename is invalid or error while opening.\n");
		return QString();
	}

	int maj, min, build, rev;

	GetVersion(maj, min, build, rev);
//	printf("\t\t libmkv verison: %d.%d.%d.%d\n", maj, min, build, rev);

	using lli = long long int;
	using clli = const lli;
	lli pos = 0;
	EBMLHeader ebmlHeader;
	ebmlHeader.Parse(&reader, pos);

//	printf("\t\t\t    EBML Header\n");
//	printf("\t\tEBML Version\t\t: %lld\n", ebmlHeader.m_version);
//	printf("\t\tEBML MaxIDLength\t: %lld\n", ebmlHeader.m_maxIdLength);
//	printf("\t\tEBML MaxSizeLength\t: %lld\n", ebmlHeader.m_maxSizeLength);
//	printf("\t\tDoc Type\t\t: %s\n", ebmlHeader.m_docType);
//	printf("\t\tPos\t\t\t: %lld\n", pos);

	mkvparser::Segment* pSegment;

	i64 ret = mkvparser::Segment::CreateInstance(&reader, pos, pSegment);
	if (ret)
	{
		printf("\n Segment::CreateInstance() failed.");
		return QString();
	}

	ret  = pSegment->Load();
	if (ret < 0)
	{
		printf("\n Segment::Load() failed.");
		return QString();
	}

	const SegmentInfo* const seg_info = pSegment->GetInfo();

	const char* const utf8_title = seg_info->GetTitleAsUTF8();
	QString ret_title = QString::fromUtf8(utf8_title);
	const wchar_t* const title = utf8towcs(utf8_title);

	if (true)
	{
		delete pSegment;
		return ret_title;
	}

	clli timeCodeScale = seg_info->GetTimeCodeScale();
	clli duration_ns = seg_info->GetDuration();

	const char* const pMuxingApp_ = seg_info->GetMuxingAppAsUTF8();
	const wchar_t* const pMuxingApp = utf8towcs(pMuxingApp_);

	const char* const pWritingApp_ = seg_info->GetWritingAppAsUTF8();
	const wchar_t* const pWritingApp = utf8towcs(pWritingApp_);

	//printf("\n");
	//printf("\t\t\t   Segment Info\n");
	//printf("\t\tTimeCodeScale\t\t: %lld \n", timeCodeScale);
	//printf("\t\tDuration\t\t: %lld\n", duration_ns);
	const double duration_sec = double(duration_ns) / 1000000000;
	//printf("\t\tDuration(secs)\t\t: %7.3lf\n", duration_sec);

	if (title) {
		//printf("\t\tTrack Name\t\t: %ls\n", title);
		delete [] title;
	} else {
		//printf("No title\n");
	}


	if (pMuxingApp == NULL) {
		//printf("\t\tMuxing App\t\t: NULL\n");
	} else {
		//printf("\t\tMuxing App\t\t: %ls\n", pMuxingApp);
		delete [] pMuxingApp;
	}

	if (pWritingApp == NULL) {
		//printf("\t\tWriting App\t\t: NULL\n");
	} else {
		//printf("\t\tWriting App\t\t: %ls\n", pWritingApp);
		delete [] pWritingApp;
	}
	// pos of segment payload
	//printf("\t\tPosition(Segment)\t: %lld\n", pSegment->m_start);

	// size of segment payload
	//printf("\t\tSize(Segment)\t\t: %lld\n", pSegment->m_size);

	const mkvparser::Tracks* tracks = pSegment->GetTracks();
	unsigned long i = 0;
	cu64 j = tracks->GetTracksCount();
	enum { VIDEO_TRACK = 1, AUDIO_TRACK = 2 };
	//printf("\n\t\t\t   Track Info\n");
while(false) //(i != j)
	{
		const Track* const pTrack = tracks->GetTrackByIndex(i++);

		if (pTrack == NULL)
			continue;

		ci64 trackType = pTrack->GetType();
		ci64 trackNumber = pTrack->GetNumber();
		cu64 trackUid = pTrack->GetUid();
		const wchar_t* const pTrackName = utf8towcs(pTrack->GetNameAsUTF8());

		printf("\t\tTrack Type\t\t: %ld\n", trackType);
		printf("\t\tTrack Number\t\t: %ld\n", trackNumber);
		printf("\t\tTrack Uid\t\t: %lu\n", trackUid);

		if (pTrackName == NULL)
			printf("\t\tTrack Name\t\t: NULL\n");
		else
		{
			printf("\t\tTrack Name\t\t: %ls \n", pTrackName);
			delete [] pTrackName;
		}

		const char* const pCodecId = pTrack->GetCodecId();

		if (pCodecId == NULL)
			printf("\t\tCodec Id\t\t: NULL\n");
		else
			printf("\t\tCodec Id\t\t: %s\n", pCodecId);

		const char* const pCodecName_ = pTrack->GetCodecNameAsUTF8();
		const wchar_t* const pCodecName = utf8towcs(pCodecName_);

		if (pCodecName == NULL)
			printf("\t\tCodec Name\t\t: NULL\n");
		else
		{
			printf("\t\tCodec Name\t\t: %ls\n", pCodecName);
			delete [] pCodecName;
		}

		if (trackType == VIDEO_TRACK)
		{
			const VideoTrack* const pVideoTrack =
			static_cast<const VideoTrack*>(pTrack);

			ci64 width =  pVideoTrack->GetWidth();
			printf("\t\tVideo Width\t\t: %ld\n", width);

			ci64 height = pVideoTrack->GetHeight();
			printf("\t\tVideo Height\t\t: %ld\n", height);

			const double rate = pVideoTrack->GetFrameRate();
			printf("\t\tVideo Rate\t\t: %f\n",rate);
		}

		if (trackType == AUDIO_TRACK)
		{
			const AudioTrack* const pAudioTrack =
			static_cast<const AudioTrack*>(pTrack);

			ci64 channels =  pAudioTrack->GetChannels();
			printf("\t\tAudio Channels\t\t: %ld\n", channels);

			ci64 bitDepth = pAudioTrack->GetBitDepth();
			printf("\t\tAudio BitDepth\t\t: %ld\n", bitDepth);

			const double sampleRate = pAudioTrack->GetSamplingRate();
			printf("\t\tAddio Sample Rate\t: %.3f\n", sampleRate);
		}
	}

	printf("\n\n\t\t\t   Cluster Info\n");
	cu64 cluster_count = pSegment->GetCount();
	printf("\t\tCluster Count\t: %ld\n\n", cluster_count);
	delete pSegment;

	if (cluster_count == 0)
	{
		printf("\t\tSegment has no clusters.\n");
		return QString();
	}

	if (ok)
		*ok = true;

	return ret_title;
}

} // cornus::mc::
