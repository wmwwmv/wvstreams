/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * WvStreamClone simply forwards all requests to the "cloned" stream.
 * 
 * NOTE: this file is a pain to maintain, because many of these functions
 * are almost (but not quite) exactly like the ones in WvStream.  If
 * WvStream changes, you need to change this too.
 * 
 * See wvstreamclone.h.
 */
#include "wvstreamclone.h"
#include <errno.h>
#include <time.h>


WvStreamClone::~WvStreamClone()
{
    if (cloned)
	delete cloned;
}


void WvStreamClone::close()
{
    if (cloned)
	cloned->close();
}


int WvStreamClone::getrfd() const
{
    if (cloned)
	return cloned->getrfd();
    return -1;
}


int WvStreamClone::getwfd() const
{
    if (cloned)
	return cloned->getwfd();
    return -1;
}


size_t WvStreamClone::uread(void *buf, size_t size)
{
    if (cloned)
	return cloned->read(buf, size);
    else
	return 0;
}


size_t WvStreamClone::uwrite(const void *buf, size_t size)
{
    // we use cloned->uwrite() here, not write(), since we want the _clone_
    // to own the output buffer, not the main stream.
    if (cloned)
	return cloned->uwrite(buf, size);
    else
	return 0;
}


bool WvStreamClone::isok() const
{
    if (errnum)
	return false;
    if (cloned)
	return cloned->isok();
    return false;
}


int WvStreamClone::geterr() const
{
    if (errnum)
	return errnum;
    if (cloned)
	return cloned->geterr();
    return EIO;
}


const char *WvStreamClone::errstr() const
{
    if (errnum)
	return WvStream::errstr();
    if (cloned)
	return cloned->errstr();
    return "No child stream!";
}


bool WvStreamClone::pre_select(SelectInfo &si)
{
    SelectRequest oldwant;
    bool result;
    time_t alarmleft = alarm_remaining();
    
    if (alarmleft == 0)
	return true; // alarm has rung
    
    if (si.wants.readable && inbuf.used() && inbuf.used() >= queue_min)
	return true;   // sure_thing if anything in WvStream buffer
    
    if (alarmleft >= 0
      && (alarmleft < si.msec_timeout || si.msec_timeout < 0))
	si.msec_timeout = alarmleft;
    
    if (cloned && cloned->isok())
    {
	oldwant = si.wants;
	
	if (!si.inherit_request)
	{
	    si.wants |= force;
	    si.wants |= cloned->force;
	}
	
	if (outbuf.used() || autoclose_time)
	    si.wants.writable = true;
	
	result = cloned->pre_select(si);
	
	si.wants = oldwant;
	return result;
    }
    
    return false;
}


bool WvStreamClone::post_select(SelectInfo &si)
{
    SelectRequest oldwant;
    bool val, want_write;
    
    if (cloned && (outbuf.used() || autoclose_time))
	flush(0);

    if (cloned && cloned->isok())
    {
	oldwant = si.wants;
	if (!si.inherit_request)
	{
	    si.wants |= force;
	    si.wants |= cloned->force;
	}

	val = cloned->post_select(si);
	want_write = si.wants.writable;
	si.wants = oldwant;
	
	// don't return true if they're looking for writable and we still
	// have data in outbuf - we're not ready to flush yet.
	if (want_write && outbuf.used())
	    return false;
	else
	{
	    if (val && si.wants.readable && read_requires_writable
	      && !read_requires_writable->select(0, false, true))
		return false;
	    if (val && si.wants.writable && write_requires_readable
	      && !write_requires_readable->select(0, true, false))
		return false;

	    return val;
	}
    }
    return false;
}


const WvAddr *WvStreamClone::src() const
{
    if (cloned)
	return cloned->src();
    return NULL;
}


void WvStreamClone::execute()
{
    WvStream::execute();
    if (cloned) cloned->callback();
}
