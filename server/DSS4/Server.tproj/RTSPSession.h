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
	File:		RTSPSession.h

	Contains:	Represents an RTSP session (duh), which I define as a complete TCP connection
				lifetime, from connection to FIN or RESET termination. This object is 
				the active element that gets scheduled and gets work done. It creates requests
				and processes them when data arrives. When it is time to close the connection
				it takes care of that.
	
*/

#ifndef __RTSPSESSION_H__
#define __RTSPSESSION_H__

#include "RTSPSessionInterface.h"
#include "RTSPRequestStream.h"
#include "RTSPRequest.h"
#include "RTPSession.h"
#include "TimeoutTask.h"

class RTSPSession : public RTSPSessionInterface
{
	public:

		RTSPSession(Bool16 doReportHTTPConnectionAddress);
		virtual ~RTSPSession();
		
		// Call this before using this object
		static void Initialize();

	private:

		SInt64 Run();
		
		// Gets & creates RTP session for this request.
		QTSS_Error	FindRTPSession(OSRefTable* inTable);
		QTSS_Error	CreateNewRTPSession(OSRefTable* inTable);
		void 		SetupClientSessionAttrs();
		
		// Does request prep & request cleanup, respectively
		void SetupRequest();
		void CleanupRequest();
		
		// Fancy random number generator
		UInt32 GenerateNewSessionID(char* ioBuffer);
		
		// Sends an error response & returns error if not ok.
		QTSS_Error IsOkToAddNewRTPSession();
		
		// Checks authentication parameters
		void CheckAuthentication();

		char				fLastRTPSessionID[QTSS_MAX_SESSION_ID_LENGTH];
		StrPtrLen			fLastRTPSessionIDPtr;

		RTSPRequest*		fRequest;
		RTPSession*			fRTPSession;
		
	
	/* -- begin adds for HTTP ProxyTunnel -- */

	// This gets grabbed whenever the input side of the session is being used.
	// It is to protect POST snarfage while input stuff is in action
	OSMutex				fReadMutex;
	
	OSRef* 				RegisterRTSPSessionIntoHTTPProxyTunnelMap(QTSS_RTSPSessionType inSessionType);
	QTSS_Error			PreFilterForHTTPProxyTunnel();				// prefilter for HTTP proxies
	Bool16 				ParseProxyTunnelHTTP();						// use by PreFilterForHTTPProxyTunnel
	void				HandleIncomingDataPacket();
		
	static 				OSRefTable* sHTTPProxyTunnelMap;	// a map of available partners.

	enum
	{
		kMaxHTTPResponseLen = 300
	};
	static				char		sHTTPResponseHeaderBuf[kMaxHTTPResponseLen];
	static 				StrPtrLen	sHTTPResponseHeaderPtr;
	static				char		*sHTTPResponseFormatStr;
	char				fProxySessionID[QTSS_MAX_SESSION_ID_LENGTH];	// our magic cookie to match proxy connections
	StrPtrLen			fProxySessionIDPtr;
		OSRef				fProxyRef;
	enum
	{
		// the kinds of HTTP Methods we're interested in for
		// RTSP tunneling
		  kHTTPMethodInit		// initialize to this
		, kHTTPMethodUnknown	// tested, but unknown
		, kHTTPMethodGet		// found one of these methods...
		, kHTTPMethodPost
	};
	
	UInt16		fHTTPMethod;
	Bool16		fWasHTTPRequest;
	Bool16		fFoundValidAccept;
	Bool16		fDoReportHTTPConnectionAddress; // true if we need to report our IP adress in reponse to the clients GET request (necessary for servers behind DNS round robin)
	/* -- end adds for HTTP ProxyTunnel -- */
	
	
		// Module invocation and module state.
		// This info keeps track of our current state so that
		// the state machine works properly.
		enum
		{
			kReadingRequest 			= 0,
			kFilteringRequest 			= 1,
			kRoutingRequest 			= 2,
			kAuthenticatingRequest		= 3,
			kAuthorizingRequest 		= 4,
			kPreprocessingRequest 		= 5,
			kProcessingRequest 			= 6,
			kSendingResponse 			= 7,
			kPostProcessingRequest		= 8,
			kCleaningUp					= 9,
		
		// states that RTSP sessions that setup RTSP
		// through HTTP tunnels pass through
			kWaitingToBindHTTPTunnel = 10,					// POST or GET side waiting to be joined with it's matching half
			kSocketHasBeenBoundIntoHTTPTunnel = 11,			// POST side after attachment by GET side ( its dying )
			kHTTPFilteringRequest = 12,						// after kReadingRequest, enter this state
			kReadingFirstRequest = 13,						// initial state - the only time we look for an HTTP tunnel
			kHaveNonTunnelMessage = 14					// we've looked at the message, and its not an HTTP tunnle message
		};
		
		UInt32 fCurrentModule;
		UInt32 fState;

		QTSS_RoleParams		fRoleParams;//module param blocks for roles.
		QTSS_ModuleState	fModuleState;
		
		QTSS_Error SetupAuthLocalPath(RTSPRequest *theRTSPRequest);
		
		
		void SaveRequestAuthorizationParams(RTSPRequest *theRTSPRequest);
		QTSS_Error DumpRequestData();

};
#endif // __RTSPSESSION_H__

