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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "getopt.h"

#include <unistd.h>
#include "OS.h"
#include "QTRTPFile.h"

#if __MACOS__
	#include <SIOUX.h>
#endif


/* getopt() replacements */
#ifdef __MACOS__
	int optind = 0;
	
	int optcharcount = 0;
	int optchar[] = { ' ' };
	
	int getopt(int argc, char *argv[], char * optstring);
	int getopt(int /*argc*/, char ** /*argv*/, char * /*optstring*/)
	{
		if( optind >= optcharcount )
			return -1;
		
		return optchar[optind++];
	}
#endif

	
int main(int argc, char *argv[]) {
	// Temporary vars
	int			ch;

	// General vars
	int				fd;
	
	const char		*MovieFilename;
	QTRTPFile		*RTPFile;
	bool			Debug = false, DeepDebug = false;
	bool			silent = false;
	bool			trackCache= false;
	bool			everytrack= false;
	bool			hintOnly = false;
    extern char* optarg;
    extern int optind;
	
/* argc/argv preloading */
#if __MACOS__
	char			*argv1 = "Movies:cruelintentions.new.mov";
	char			*argv2 = "5";
	char			*argv3 = "6";
	
	argc = 3;
	argv[0] = argv1;
	argv[1] = argv2;
	argv[2] = argv3;
#endif

/* SIOUX setup */
#if __MACOS__
	SIOUXSettings.asktosaveonclose = 0;
#endif

	//
	// Read our command line options
	while( (ch = getopt(argc, argv, "dDhset")) != -1 ) {
		switch( ch ) {
			case 'e':
				everytrack = true;
			break;

			case 's':
				silent = true;
			break;

			case 't':
				trackCache = true;
			break;
			
			case 'h':
				hintOnly = true;
			break;

			case 'd':
				Debug = true;
			break;

			case 'D':
				Debug = true;
				DeepDebug = true;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	//
	// Validate our arguments.
	if( argc < 1 ) {
		printf("usage: QTRTPFileTest <filename> <track#n> <track#n+1> ..\n");
		printf("usage: -s no packet printfs\n");
		printf("usage: -e test every hint track\n");
		printf("usage: -t write packets to track.cache file\n");
		printf("usage: -h show hinted (.unopt, .opt)\n");
		exit(1);
	}
	
	MovieFilename = *argv++;
	argc--;

	if (!hintOnly)
		printf("****************** QTRTPFileTest ******************\n");

	//
	// Open the movie.
	RTPFile = new QTRTPFile(Debug, DeepDebug);
	switch( RTPFile->Initialize(MovieFilename) ) {
		case QTRTPFile::errNoError:
	    case QTRTPFile::errNoHintTracks:
		break;

		case QTRTPFile::errFileNotFound:
			printf("Error!  File not found \"%s\"!\n", MovieFilename);
			exit(1);
		case QTRTPFile::errInvalidQuickTimeFile:
			printf("Error!  Invalid movie file \"%s\"!\n", MovieFilename);
			exit(1);
		case QTRTPFile::errInternalError:
			printf("Error!  Internal error opening movie file \"%s\"!\n", MovieFilename);
			exit(1);
        case QTRTPFile::errTrackIDNotFound:
        case QTRTPFile::errCallAgain:
            //noops
            break;
	}
	
	
	//
	// Get the SDP file and print it out.
	char		*SDPFile;
	int			SDPFileLength;

	{
		//
		// Get the file
		SDPFile = RTPFile->GetSDPFile(&SDPFileLength);
		if( SDPFile == NULL ) {
			printf("Error!  Could not get SDP file!\n");
			exit(1);
		}
		if (!hintOnly)
		{
			write(1, SDPFile, SDPFileLength);
			write(1, "\n", 1);
		}
	}

	//
	// Open our file to write the packets out to.
	if (trackCache)
	{
		fd = open("track.cache", O_CREAT | O_TRUNC | O_WRONLY, 0664);
		if( fd == -1 ) {
			printf("Error!  Could not create output file!\n");
			exit(1);
		}
	}
	if (everytrack || hintOnly)
	{
		int trackcount = 0;
		int hinttracks[20];
		memset(&hinttracks,0,sizeof(hinttracks));
		bool found = false;
		char *trackPtr = SDPFile;
		while (true) 
		{	
			trackPtr = ::strstr(trackPtr,"trackID=");
			if (trackPtr != NULL)
			{	trackPtr+= ::strlen("trackID=");
				sscanf(trackPtr, "%d",&hinttracks[trackcount]);
				trackcount++;
				found = true;
			}
			else 
				break;
			
		}
		while (trackcount)
		{
			switch( RTPFile->AddTrack(hinttracks[trackcount -1]) ) {
				case QTRTPFile::errNoError:
		        case QTRTPFile::errNoHintTracks:
				break;

				case QTRTPFile::errFileNotFound:
				case QTRTPFile::errInvalidQuickTimeFile:
				case QTRTPFile::errInternalError:
					printf("Error!  Invalid movie file \"%s\"!\n", MovieFilename);
					exit(1);
                    
				case QTRTPFile::errTrackIDNotFound:
				case QTRTPFile::errCallAgain:
                    //noops
                break;
			}

			RTPFile->SetTrackCookies(hinttracks[trackcount], (char *)hinttracks[trackcount], 0);
			(void)RTPFile->GetSeekTimestamp(hinttracks[trackcount]);
			trackcount --;
		}
	}
	else
	{
		//
		// Add the tracks that we're interested in.
		while(argc--) {
			switch( RTPFile->AddTrack(atoi(*argv)) ) {
				case QTRTPFile::errNoError:
		        case QTRTPFile::errNoHintTracks:
		        case QTRTPFile::errTrackIDNotFound:
		        case QTRTPFile::errCallAgain:
				break;

				case QTRTPFile::errFileNotFound:
				case QTRTPFile::errInvalidQuickTimeFile:
				case QTRTPFile::errInternalError:
					printf("Error!  Invalid movie file \"%s\"!\n", MovieFilename);
					exit(1);
			}

			RTPFile->SetTrackCookies(atoi(*argv), (char *)atoi(*argv), 0);
			(void)RTPFile->GetSeekTimestamp(atoi(*argv));
			argv++;
		}
	}
	
	
	//
	// Display some stats about the movie.
	
	if (!hintOnly)
		printf("Total RTP bytes of all added tracks: %"_64BITARG_"u\n", RTPFile->GetAddedTracksRTPBytes());

	//
	// Seek to the beginning of the movie.
	if( RTPFile->Seek(0.0) != QTRTPFile::errNoError ) {
		printf("Error!  Couldn't seek to time 0.0!\n");
		exit(1);
	}
	
	//
	// Suck down packets..
	UInt32		NumberOfPackets = 0;
	Float64		TotalInterpacketDelay = 0.0,
				LastPacketTime = 0.0;
				
	SInt64 startTime = 0;
	SInt64 durationTime = 0;
	SInt64 packetCount = 0;

	while(1) 
	{
		// Temporary vars
		UInt16	tempInt16;

		// General vars
		char	*Packet;
		int		PacketLength;
		//long	Cookie;
		UInt32	RTPTimestamp;
		UInt16	RTPSequenceNumber;
		int		maxHintPackets = 100; // cheat assume this many packets is good enough to assume entire file is the same at these packets
	

		//
		// Get the next packet.
		startTime = OS::Milliseconds();
		Float64 TransmitTime = RTPFile->GetNextPacket(&Packet, &PacketLength);
		SInt64 thisDuration = OS::Milliseconds() - startTime;
		durationTime += thisDuration;
		packetCount++;

		if( Packet == NULL )
			break;
		
		if (hintOnly)
		{	if (--maxHintPackets == 0 )	  
				break;
			continue;
		}
			
		memcpy(&RTPSequenceNumber, Packet + 2, 2);
		RTPSequenceNumber = ntohs(RTPSequenceNumber);
		memcpy(&RTPTimestamp, Packet + 4, 4);
		RTPTimestamp = ntohl(RTPTimestamp);
		
		if (!hintOnly)
			if (!silent)
				printf("TransmitTime = %.2f; SEQ = %u; TS = %lu\n", TransmitTime, RTPSequenceNumber, RTPTimestamp);
		
		if (trackCache)
		{
			//
			// Write out the packet header.
			write(fd, (char *)&TransmitTime, 8);	// transmitTime
			tempInt16 = PacketLength;
			write(fd, (char *)&tempInt16, 2);		// packetLength
			tempInt16 = 0;
			write(fd, (char *)&tempInt16, 2);		// padding1
			
			//
			// Write out the packet.
			write(fd, Packet, PacketLength);
		}
		//
		// Compute the Inter-packet delay and keep track of it.
		if( TransmitTime >= LastPacketTime ) {
			TotalInterpacketDelay += TransmitTime - LastPacketTime;
			LastPacketTime = TransmitTime;
			NumberOfPackets++;
		}
		
	}


	//
	// Compute and display the Inter-packet delay.
	if( (!hintOnly) && NumberOfPackets > 0 )
	{
		printf("QTRTPFileTest: Total GetNextPacket durationTime = %lums packetCount= %lu\n",(UInt32)durationTime,(UInt32)packetCount);		
		printf("QTRTPFileTest: Average Inter-packet delay: %"_64BITARG_"uus\n", (UInt64)((TotalInterpacketDelay / NumberOfPackets) * 1000 * 1000));
	}	
	
	SInt32 hintType = RTPFile->GetMovieHintType(); // this can only be reliably called after playing all the packets. 
	if (!hintOnly)
	{
		if (hintType < 0) 
			printf("QTRTPFileTest: HintType=Optimized\n");
		else if (hintType > 0) 
			printf("QTRTPFileTest: HintType=Unoptimized\n");
		else 
			printf("QTRTPFileTest: HintType=Unknown\n");
	}
	else
	{
		if (hintType < 0) 
			printf("%s.opt\n",MovieFilename);
		else if (hintType > 0) 
			printf("%s.unopt\n",MovieFilename);
		else 
			printf("%s\n",MovieFilename);
	}		
	
	if (trackCache)
	{	//
		// Close the output file.
		close(fd);
	}
	
	//
	// Close our RTP file.
	delete RTPFile;

	return 0;
}
