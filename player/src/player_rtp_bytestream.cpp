/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is MPEG4IP.
 * 
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2000, 2001.  All Rights Reserved.
 * 
 * Contributor(s): 
 *              Bill May        wmay@cisco.com
 */
/*
 * player_rtp_bytestream.cpp - provides a RTP based bytestream
 */

#include "player_rtp_bytestream.h"
#include "player_media.h"

CInByteStreamRtp::CInByteStreamRtp (CPlayerMedia *m, int ondemand) : 
  COurInByteStream(m)
{
  init();
  m_ts = 0;
  m_total =0;
  m_dont_check_across_ts = 0;
  m_skip_on_advance_bytes = 0;
  m_stream_ondemand = ondemand;
  m_wrap_offset = 0;
  m_wallclock_offset_set = 0;
}

CInByteStreamRtp::~CInByteStreamRtp()
{

}

void CInByteStreamRtp::init()
{
  m_pak = NULL;
  m_offset_in_pak = 0;
  m_bookmark_set = 0;
}

void CInByteStreamRtp::assign_pak (rtp_packet *pak)
{
  const char *err;
  while (pak->data_len == 0) {
    pak = m_media->advance_head(FALSE, &err);
  }
  m_pak = pak; 
  m_offset_in_pak = m_skip_on_advance_bytes;;
}

void CInByteStreamRtp::check_for_end_of_pak (void)
{
  const char *err;

  if (m_offset_in_pak >= m_pak->data_len) {
    m_offset_in_pak = m_skip_on_advance_bytes;
    m_pak = m_media->advance_head(m_bookmark_set, &err);

    if (m_pak == NULL && m_bookmark_set == 0) {
      init();
      throw err;
    } 
    //player_debug_message("Advancing head");
  } else {
    //player_debug_message("Rtp offset %u", m_offset_in_pak);
  }
}

unsigned char CInByteStreamRtp::get (void)
{
  unsigned char ret;

  if (m_pak == NULL) {
    if (m_bookmark_set == 1) {
      return (0);
    }
    init();
    throw "NULL when start";
  }

  if ((m_offset_in_pak == 0) &&
      (m_dont_check_across_ts == 0) &&
      (m_ts != m_pak->ts)) {
    if (m_bookmark_set == 1) {
      return (0);
    }
    throw("DECODE ACROSS TS");
  }
  ret = m_pak->data[m_offset_in_pak];
  m_offset_in_pak++;
  m_total++;
  check_for_end_of_pak();
  return (ret);
}

unsigned char CInByteStreamRtp::peek (void) 
{
  return (m_pak ? m_pak->data[m_offset_in_pak] : 0);
}

void CInByteStreamRtp::bookmark (int bSet)
{
  if (bSet == TRUE) {
    m_bookmark_set = 1;
    m_bookmark_pak = m_pak;
    m_bookmark_offset_in_pak = m_offset_in_pak;
    m_total_book = m_total;
    //player_debug_message("bookmark on");
  } else {
    m_bookmark_set = 0;
    m_pak= m_bookmark_pak;
    m_offset_in_pak = m_bookmark_offset_in_pak;
    m_total = m_total_book;
    //player_debug_message("book restore %d", m_offset_in_pak);
  }
}

uint64_t CInByteStreamRtp::start_next_frame (void)
{
  uint64_t timetick;
  // We're going to have to handle wrap better...
  if (m_stream_ondemand) {
    m_ts = m_pak->ts;
    int neg = 0;
    
    if (m_ts >= m_rtp_rtptime) {
      timetick = m_ts;
      timetick -= m_rtp_rtptime;
    } else {
      timetick = m_rtp_rtptime - m_ts;
      neg = 1;
    }
    timetick *= 1000;
    timetick /= m_rtptime_tickpersec;
    if (neg == 1) {
      if (timetick > m_play_start_time) return (0);
      return (m_play_start_time - timetick);
    }
    timetick += m_play_start_time;
  } else {
    if (((m_ts & 0x80000000) == 0x80000000) &&
	((m_pak->ts & 0x80000000) == 0)) {
      m_wrap_offset += (I_LLU << 32);
    }
    m_ts = m_pak->ts;
    timetick = m_ts + m_wrap_offset;
    timetick *= 1000;
    timetick /= m_rtptime_tickpersec;
    timetick += m_wallclock_offset;
#if 0
	player_debug_message("time %x "LLX" "LLX" "LLX" "LLX,
			m_ts, m_wrap_offset, m_rtptime_tickpersec, m_wallclock_offset,
			timetick);
#endif
  }
  return (timetick);
					   
}

size_t CInByteStreamRtp::read (unsigned char *buffer, size_t bytes_to_read)
{
  size_t inbuffer, readbytes = 0;

  if (m_pak == NULL) {
    if (m_bookmark_set == 1) {
      return (0);
    }
    init();
    throw "NULL when start";
  }

  do {
    if ((m_offset_in_pak == 0) &&
	(m_dont_check_across_ts == 0) &&
	(m_ts != m_pak->ts)) {
      if (m_bookmark_set == 1) {
	return (0);
      }
      throw("DECODE ACROSS TS");
    }
    inbuffer = m_pak->data_len - m_offset_in_pak;
    if (bytes_to_read < inbuffer) {
      inbuffer = bytes_to_read;
    }
    memcpy(buffer, &m_pak->data[m_offset_in_pak], inbuffer);
    buffer += inbuffer;
    bytes_to_read -= inbuffer;
    m_offset_in_pak += inbuffer;
    m_total += inbuffer;
    readbytes += inbuffer;
    check_for_end_of_pak();
  } while (bytes_to_read > 0 && m_pak != NULL);
  return (readbytes);
}

int CInByteStreamRtp::have_no_data (void)
{
  if (m_pak == NULL) {
    m_pak = m_media->get_rtp_head();
  }
  return (m_pak == NULL);
}

int CInByteStreamRtp::still_same_ts (void) 
{
    rtp_packet *head;
    head = m_media->get_rtp_head();
    return (head && head->ts == m_ts); 
}

/* end player_rtp_bytestream.cpp */
