/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@devolution.com
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id: SDL_blit.c,v 1.3 2001/05/09 21:15:10 cahighlander Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_sysvideo.h"
#include "SDL_blit.h"
#include "SDL_RLEaccel_c.h"
#include "SDL_pixels_c.h"
#include "SDL_memops.h"

/* The general purpose software blit routine */
static int SDL_SoftBlit(SDL_Surface *src, SDL_Rect *srcrect,
			SDL_Surface *dst, SDL_Rect *dstrect)
{
	int okay;
	int src_locked;
	int dst_locked;

	/* Everything is okay at the beginning...  */
	okay = 1;

	/* Lock the destination if it's in hardware */
	dst_locked = 0;
	if ( dst->flags & (SDL_HWSURFACE|SDL_ASYNCBLIT) ) {
		SDL_VideoDevice *video = current_video;
		SDL_VideoDevice *this  = current_video;
		if ( video->LockHWSurface(this, dst) < 0 ) {
			okay = 0;
		} else {
			dst_locked = 1;
		}
	}
	/* Lock the source if it's in hardware */
	src_locked = 0;
	if ( src->flags & (SDL_HWSURFACE|SDL_ASYNCBLIT) ) {
		SDL_VideoDevice *video = current_video;
		SDL_VideoDevice *this  = current_video;
		if ( video->LockHWSurface(this, src) < 0 ) {
			okay = 0;
		} else {
			src_locked = 1;
		}
	}

	/* Unencode the destination if it's RLE encoded */
	if ( dst->flags & SDL_RLEACCEL ) {
		SDL_UnRLESurface(dst, 1);
		dst->flags |= SDL_RLEACCEL;	/* save accel'd state */
	}

	/* Set up source and destination buffer pointers, and BLIT! */
	if ( okay  && srcrect->w && srcrect->h ) {
		SDL_BlitInfo info;
		SDL_loblit RunBlit;

		/* Set up the blit information */
		info.s_pixels = (Uint8 *)src->pixels + src->offset +
				(Uint16)srcrect->y*src->pitch +
				(Uint16)srcrect->x*src->format->BytesPerPixel;
		info.s_width = srcrect->w;
		info.s_height = srcrect->h;
		info.s_skip=src->pitch-info.s_width*src->format->BytesPerPixel;
		info.d_pixels = (Uint8 *)dst->pixels + dst->offset +
				(Uint16)dstrect->y*dst->pitch +
				(Uint16)dstrect->x*dst->format->BytesPerPixel;
		info.d_width = dstrect->w;
		info.d_height = dstrect->h;
		info.d_skip=dst->pitch-info.d_width*dst->format->BytesPerPixel;
		info.aux_data = src->map->sw_data->aux_data;
		info.src = src->format;
		info.table = src->map->table;
		info.dst = dst->format;
		RunBlit = src->map->sw_data->blit;

		/* Run the actual software blit */
		RunBlit(&info);
	}

	/* Re-encode the destination if it's RLE encoded */
	if ( dst->flags & SDL_RLEACCEL ) {
	        dst->flags &= ~SDL_RLEACCEL; /* stop lying */
		SDL_RLESurface(dst);
	}

	/* We need to unlock the surfaces if they're locked */
	if ( dst_locked ) {
		SDL_VideoDevice *video = current_video;
		SDL_VideoDevice *this  = current_video;
		video->UnlockHWSurface(this, dst);
	} else
	if ( src_locked ) {
		SDL_VideoDevice *video = current_video;
		SDL_VideoDevice *this  = current_video;
		video->UnlockHWSurface(this, src);
	}
	/* Blit is done! */
	return(okay ? 0 : -1);
}

static void SDL_BlitCopy(SDL_BlitInfo *info)
{
	Uint8 *src, *dst;
	int w, h;
	int srcskip, dstskip;

	w = info->d_width*info->dst->BytesPerPixel;
	h = info->d_height;
	src = info->s_pixels;
	dst = info->d_pixels;
	srcskip = w+info->s_skip;
	dstskip = w+info->d_skip;
	while ( h-- ) {
		SDL_memcpy(dst, src, w);
		src += srcskip;
		dst += dstskip;
	}
}

static void SDL_BlitCopyOverlap(SDL_BlitInfo *info)
{
	Uint8 *src, *dst;
	int w, h;
	int srcskip, dstskip;

	w = info->d_width*info->dst->BytesPerPixel;
	h = info->d_height;
	src = info->s_pixels;
	dst = info->d_pixels;
	srcskip = w+info->s_skip;
	dstskip = w+info->d_skip;
	if ( dst < src ) {
		while ( h-- ) {
			SDL_memcpy(dst, src, w);
			src += srcskip;
			dst += dstskip;
		}
	} else {
		src += ((h-1) * srcskip);
		dst += ((h-1) * dstskip);
		while ( h-- ) {
			SDL_revcpy(dst, src, w);
			src -= srcskip;
			dst -= dstskip;
		}
	}
}

/* Figure out which of many blit routines to set up on a surface */
int SDL_CalculateBlit(SDL_Surface *surface)
{
	int blit_index;

	/* Clean everything out to start */
	if ( (surface->flags & SDL_RLEACCEL) == SDL_RLEACCEL ) {
		SDL_UnRLESurface(surface, 1);
	}
	surface->map->sw_blit = NULL;

	/* Figure out if an accelerated hardware blit is possible */
	surface->flags &= ~SDL_HWACCEL;
	if ( surface->map->identity ) {
		int hw_blit_ok;

		if ( (surface->flags & SDL_HWSURFACE) == SDL_HWSURFACE ) {
			/* We only support accelerated blitting to hardware */
			if ( surface->map->dst->flags & SDL_HWSURFACE ) {
				hw_blit_ok = current_video->info.blit_hw;
			} else {
				hw_blit_ok = 0;
			}
			if (hw_blit_ok && (surface->flags & SDL_SRCCOLORKEY)) {
				hw_blit_ok = current_video->info.blit_hw_CC;
			}
			if ( hw_blit_ok && (surface->flags & SDL_SRCALPHA) ) {
				hw_blit_ok = current_video->info.blit_hw_A;
			}
		} else {
			/* We only support accelerated blitting to hardware */
			if ( surface->map->dst->flags & SDL_HWSURFACE ) {
				hw_blit_ok = current_video->info.blit_sw;
			} else {
				hw_blit_ok = 0;
			}
			if (hw_blit_ok && (surface->flags & SDL_SRCCOLORKEY)) {
				hw_blit_ok = current_video->info.blit_sw_CC;
			}
			if ( hw_blit_ok && (surface->flags & SDL_SRCALPHA) ) {
				hw_blit_ok = current_video->info.blit_sw_A;
			}
		}
		if ( hw_blit_ok ) {
			SDL_VideoDevice *video = current_video;
			SDL_VideoDevice *this  = current_video;
			video->CheckHWBlit(this, surface, surface->map->dst);
		}
	}

	/* Get the blit function index, based on surface mode */
	/* { 0 = nothing, 1 = colorkey, 2 = alpha, 3 = colorkey+alpha } */
	blit_index = 0;
	blit_index |= (!!(surface->flags & SDL_SRCCOLORKEY))      << 0;
	if ( surface->flags & SDL_SRCALPHA
	     && (surface->format->alpha != SDL_ALPHA_OPAQUE
		 || surface->format->Amask) ) {
	        blit_index |= 2;
	}

	/* Check for special "identity" case -- copy blit */
	if ( surface->map->identity && blit_index == 0 ) {
	        surface->map->sw_data->blit = SDL_BlitCopy;

		/* Handle overlapping blits on the same surface */
		if ( surface == surface->map->dst ) {
		        surface->map->sw_data->blit = SDL_BlitCopyOverlap;
		}
	} else {
		if ( surface->format->BitsPerPixel < 8 ) {
			surface->map->sw_data->blit =
			    SDL_CalculateBlit0(surface, blit_index);
		} else {
			switch ( surface->format->BytesPerPixel ) {
			    case 1:
				surface->map->sw_data->blit =
				    SDL_CalculateBlit1(surface, blit_index);
				break;
			    case 2:
			    case 3:
			    case 4:
				surface->map->sw_data->blit =
				    SDL_CalculateBlitN(surface, blit_index);
				break;
			    default:
				surface->map->sw_data->blit = NULL;
				break;
			}
		}
	}
	/* Make sure we have a blit function */
	if ( surface->map->sw_data->blit == NULL ) {
		SDL_InvalidateMap(surface->map);
		SDL_SetError("Blit combination not supported");
		return(-1);
	}

	/* Choose software blitting function */
	if(surface->flags & SDL_RLEACCELOK
	   && (surface->flags & SDL_HWACCEL) != SDL_HWACCEL) {

	        if(surface->map->identity
		   && (blit_index == 1
		       || (blit_index == 3 && !surface->format->Amask))) {
		        if ( SDL_RLESurface(surface) == 0 )
			        surface->map->sw_blit = SDL_RLEBlit;
		} else if(blit_index == 2 && surface->format->Amask) {
		        if ( SDL_RLESurface(surface) == 0 )
			        surface->map->sw_blit = SDL_RLEAlphaBlit;
		}
	}
	
	if ( surface->map->sw_blit == NULL ) {
		surface->map->sw_blit = SDL_SoftBlit;
	}
	return(0);
}

