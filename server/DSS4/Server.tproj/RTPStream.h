/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2001 Apple Computer, Inc.  All Rights Reserved. The
 * contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License.  Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 *
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */
/*
	File:		RTPStream.h

	Contains:	Represents a single client stream (audio, video, etc).
				Control API is similar to overall session API.
				
				Contains all stream-specific data & resources, used by Session when it
				wants to send out or receive data for this stream
				
				This is also the class that implements the RTP stream dictionary
				for QTSS API.
				

*/

#ifndef __RTPSTREAM_H__
#define __RTPSTREAM_H__

#include "QTSS.h"
#include "QTSSDictionary.h"
#include "QTSS_Private.h"

#include "UDPDemuxer.h"
#include "UDPSocketPool.h"

#include "RTSPRequestInterface.h"
#include "RTPSessionInterface.h"

#include "RTPPacketResender.h"

class RTPStream : public QTSSDictionary, public UDPDemuxerTask
{
	public:
		
		// Initializes dictionary resources
		static void	Initialize();

		//
		// CONSTRUCTOR / DESTRUCTOR
		
		RTPStream(UInt32 inSSRC, RTPSessionInterface* inSession);
		virtual ~RTPStream();
		
		//
		//ACCESS FUNCTIONS
		
		UInt32		GetSSRC()					{ return fSsrc; }
		UInt8		GetRTPChannelNum()			{ return fRTPChannel; }
		UInt8		GetRTCPChannelNum()			{ return fRTCPChannel; }
		RTPPacketResender* GetResender()		{ return &fResender; }
		QTSS_RTPTransportType GetTransportType() { return fTransportType; }
		UInt32		GetStalePacketsDropped()	{ return fStalePacketsDropped; }
		// Setup uses the info in the RTSPRequestInterface to associate
		// all the necessary resources, ports, sockets, etc, etc, with this
		// stream.
		QTSS_Error Setup(RTSPRequestInterface* request, QTSS_AddStreamFlags inFlags);
		
		// Write sends RTP data to the client. Caller must specify
		// either qtssWriteFlagsIsRTP or qtssWriteFlagsIsRTCP
		virtual QTSS_Error	Write(void* inBuffer, UInt32 inLen,
										UInt32* outLenWritten, QTSS_WriteFlags inFlags);
		
		
		//UTILITY FUNCTIONS:
		//These are not necessary to call and do not manipulate the state of the
		//stream. They may, however, be useful services exported by the server
		
		// Formats a standard setup response.
		void			SendSetupResponse(RTSPRequestInterface* request);

		//Formats a transport header for this stream. 
		void			AppendTransport(RTSPRequestInterface* request);
		
		//Formats a RTP-Info header for this stream.
		//Isn't useful unless you've already called Play()
		void			AppendRTPInfo(QTSS_RTSPHeader inHeader,
										RTSPRequestInterface* request, UInt32 inFlags, Bool16 lastInfo);

		//
		// When we get an incoming Interleaved Packet for this stream, this
		// function should be called
		void ProcessIncomingInterleavedData(UInt8 inChannelNum, RTSPSessionInterface* inRTSPSession, StrPtrLen* inPacket);

		//When we get a new RTCP packet, we can directly invoke the RTP session and tell it
		//to process the packet right now!
		void ProcessIncomingRTCPPacket(StrPtrLen* inPacket);

		// Send a RTCP SR on this stream. Pass in true if this SR should also have a BYE
		void SendRTCPSR(const SInt64& inTime, Bool16 inAppendBye = false);
		
		//
		// Retransmits get sent when there is new data to be sent, but this function
		// should be called periodically even if there is no new packet data, as
		// the pipe should have a steady stream of data in it. 
		void SendRetransmits();

		//
		// Update the thinning parameters for this stream to match current prefs
		void SetThinningParams();
		
	private:
		
		enum
		{
			kMaxSsrcSizeInBytes 		= 12,
			kMaxStreamURLSizeInBytes 	= 32,
			kDefaultPayloadBufSize		= 32,
			kSenderReportIntervalInSecs = 7,
			kNumPrebuiltChNums			= 10,
		};
	
		SInt64 fLastQualityChange;
		SInt32 fQualityInterval;

		//either pointers to the statically allocated sockets (maintained by the server)
		//or fresh ones (only fresh in extreme special cases)
		UDPSocketPair* 			fSockets;
		RTPSessionInterface*	fSession;

		// info for kinda reliable UDP
		//DssDurationTimer		fInfoDisplayTimer;
		SInt32					fBytesSentThisInterval;
		SInt32					fDisplayCount;
		Bool16					fSawFirstPacket;
		SInt64					fStreamCumDuration;
		// manages UDP retransmits
		RTPPacketResender		fResender;
		RTPBandwidthTracker*	fTracker;

		
		//who am i sending to?
		UInt32		fRemoteAddr;
		UInt16		fRemoteRTPPort;
		UInt16		fRemoteRTCPPort;
		UInt16		fLocalRTPPort;
		
		//RTCP stuff 
		SInt64		fLastSenderReportTime;
		UInt32		fPacketCount;
		UInt32		fLastPacketCount;
		UInt32		fPacketCountInRTCPInterval;
		UInt32		fByteCount;
		
		// DICTIONARY ATTRIBUTES
		
		//Module assigns a streamID to this object
		UInt32		fTrackID;
		
		//low-level RTP stuff 
		UInt32		fSsrc;
		char		fSsrcString[kMaxSsrcSizeInBytes];
		StrPtrLen	fSsrcStringPtr;
		
		//Payload name and codec type.
		char				fPayloadNameBuf[kDefaultPayloadBufSize];
		QTSS_RTPPayloadType	fPayloadType;

		//Media information.
		UInt16		fFirstSeqNumber;//used in sending the play response
		UInt32		fFirstTimeStamp;//RTP time
		UInt32		fTimescale;
		
		//what is the URL for this stream?
		char		fStreamURL[kMaxStreamURLSizeInBytes];
		StrPtrLen	fStreamURLPtr;
		
		SInt32		fQualityLevel;
		UInt32		fNumQualityLevels;
		
		UInt32		fLastRTPTimestamp;
		
		// RTCP data
		UInt32		fFractionLostPackets;
		UInt32		fTotalLostPackets;
		UInt32		fJitter;
		UInt32		fReceiverBitRate;
		UInt16		fAvgLateMsec;
		UInt16		fPercentPacketsLost;
		UInt16		fAvgBufDelayMsec;
		UInt16		fIsGettingBetter;
		UInt16		fIsGettingWorse;
		UInt32		fNumEyes;
		UInt32		fNumEyesActive;
		UInt32		fNumEyesPaused;
		UInt32		fTotalPacketsRecv;
		UInt32		fPriorTotalPacketsRecv;
		UInt16		fTotalPacketsDropped;
		UInt16		fTotalPacketsLost;
		UInt32		fCurPacketsLostInRTCPInterval;
		UInt16		fClientBufferFill;
		UInt16		fFrameRate;
		UInt16		fExpectedFrameRate;
		UInt16		fAudioDryCount;
		UInt32		fClientSSRC;
		
		Bool16		fIsTCP;
		QTSS_RTPTransportType	fTransportType;
		
		// HTTP params
		// Each stream has a set of thinning related tolerances,
		// that are dependent on prefs and parameters in the SETUP.
		// These params, as well as the current packet delay determine
		// whether a packet gets dropped.
		SInt32		fTurnThinningOffDelay_TCP;
		SInt32		fIncreaseThinningDelay_TCP;
		SInt32		fDropAllPacketsForThisStreamDelay_TCP;
		UInt32		fStalePacketsDropped_TCP;
		SInt64		fTimeStreamCaughtUp_TCP;
		SInt64		fLastQualityLevelIncreaseTime_TCP;
		//
		// Each stream has a set of thinning related tolerances,
		// that are dependent on prefs and parameters in the SETUP.
		// These params, as well as the current packet delay determine
		// whether a packet gets dropped.
		SInt32		fThinAllTheWayDelay;
		SInt32		fAlwaysThinDelay;
		SInt32		fStartThinningDelay;
		SInt32		fStartThickingDelay;
		SInt32		fThickAllTheWayDelay;
		SInt32		fQualityCheckInterval;
		SInt32		fDropAllPacketsForThisStreamDelay;
		UInt32		fStalePacketsDropped;
		SInt64		fLastCurrentPacketDelay;
		Bool16		fWaitOnLevelAdjustment;
		
		Float32 	fBufferDelay; // from the sdp
		Float32		fLateToleranceInSec;
				
		// Pointer to the stream ref (this is just a this pointer)
		QTSS_StreamRef	fStreamRef;
		
		UInt32		fCurrentAckTimeout;
		SInt32		fMaxSendAheadTimeMSec;
		
#if DEBUG
		UInt32		fNumPacketsDroppedOnTCPFlowControl;
		SInt64		fFlowControlStartedMsec;
		SInt64		fFlowControlDurationMsec;
#endif
		
		// If we are interleaving RTP data over the TCP connection,
		// these are channel numbers to use for RTP & RTCP
		UInt8	fRTPChannel;
		UInt8	fRTCPChannel;
		
		QTSS_RTPNetworkMode		fNetworkMode;
		
		// acutally write the data out that way
		QTSS_Error	InterleavedWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, unsigned char channel );

		// implements the ReliableRTP protocol
		QTSS_Error	ReliableRTPWrite(void* inBuffer, UInt32 inLen, const SInt64& curPacketDelay);

		void 		SetTCPThinningParams();
		QTSS_Error	TCPWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, UInt32 inFlags);

		static QTSSAttrInfoDict::AttrInfo	sAttributes[];
		static StrPtrLen					sChannelNums[];
		static QTSS_ModuleState				sRTCPProcessModuleState;
		
		Bool16 UpdateQualityLevel(const SInt64& inTransmitTime, const SInt64& inCurrentPacketDelay,
										const SInt64& inCurrentTime, UInt32 inPacketSize);
		
		SInt32			GetQualityLevel();
		void			SetQualityLevel(SInt32 level);
};

#endif // __RTPSTREAM_H__
