/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 * 
 * WvIStreamList holds a list of IWvStream objects -- and its select() and
 * callback() functions know how to handle multiple simultaneous streams.
 */
#include "wvistreamlist.h"
#include "wvstringlist.h"
#include "wvstreamsdebugger.h"
#include "wvstrutils.h"

#include "wvassert.h"
#include "wvstrutils.h"

#ifndef _WIN32
#include "wvfork.h"
#endif

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#else
#define RUNNING_ON_VALGRIND false
#endif

// enable this to add some read/write trace messages (this can be VERY
// verbose)
#define STREAMTRACE 0
#if STREAMTRACE
# define TRACE(x, y...) fprintf(stderr, x, ## y)
#else
#ifndef _MSC_VER
# define TRACE(x, y...)
#else
# define TRACE
#endif
#endif

WvIStreamList WvIStreamList::globallist;


WvIStreamList::WvIStreamList():
    in_select(false), dead_stream(false)
{
    readcb = writecb = exceptcb = 0;
    auto_prune = true;
    if (this == &globallist)
    {
	globalstream = this;
#ifndef _WIN32
        add_wvfork_callback(WvIStreamList::onfork);
#endif
        set_wsname("globallist");
        add_debugger_commands();
    }
}


WvIStreamList::~WvIStreamList()
{
    // nothing to do
}


bool WvIStreamList::isok() const
{
    return true;  // "error" condition on a list is undefined
}


class BoolGuard
{
public:
    BoolGuard(bool &_guard_bool):
	guard_bool(_guard_bool)
    {
	assert(!guard_bool);
	guard_bool = true;
    }
    ~BoolGuard()
    {
	guard_bool = false;
    }
private:
    bool &guard_bool;
};


bool WvIStreamList::pre_select(SelectInfo &si)
{
    //BoolGuard guard(in_select);
    bool already_sure = false;
    SelectRequest oldwant;
    
    dead_stream = false;
    
    sure_thing.zap();
    
    time_t alarmleft = alarm_remaining();
    if (alarmleft == 0)
	already_sure = true;
    
    oldwant = si.wants;
    
    Iter i(*this);
    for (i.rewind(); i.next(); )
    {
	IWvStream &s(*i);
        #if I_ENJOY_FORMATTING_STRINGS
	WvCrashWill will("doing pre_select for \"%s\" (%s)\n%s",
			 i.link->id, ptr2str(&s), wvcrash_read_will());
        #endif
	
        si.wants = oldwant;

	if (!s.isok())
	{
	    dead_stream = true;
	    already_sure = true;
	    if (auto_prune)
		i.xunlink();
	    continue;
	}
	else if (s.pre_select(si))
	{
	    // printf("pre_select sure_thing: '%s'\n", i.link->id);
	    sure_thing.append(&s, false, i.link->id);
            #if I_ENJOY_FORMATTING_STRINGS
	    wvassert(si.msec_timeout == 0, "pre_select for \"%s\" (%s) "
		     "returned true, but has non-zero timeout",
		     i.link->id, ptr2str(&s));
            #endif
	}
    }

    if (alarmleft >= 0 && (alarmleft < si.msec_timeout || si.msec_timeout < 0))
	si.msec_timeout = alarmleft;
    
    si.wants = oldwant;

    if (already_sure || !sure_thing.isempty())
	si.msec_timeout = 0;

    return already_sure || !sure_thing.isempty();
}


bool WvIStreamList::post_select(SelectInfo &si)
{
    //BoolGuard guard(in_select);
    bool already_sure = dead_stream;
    SelectRequest oldwant = si.wants;
    
    dead_stream = false;

    time_t alarmleft = alarm_remaining();
    if (alarmleft == 0)
	already_sure = true;

    Iter i(*this);
    for (i.rewind(); i.cur() && i.next(); )
    {
	IWvStream &s(*i);
        #if I_ENJOY_FORMATTING_STRINGS
	WvCrashWill will("doing post_select for \"%s\" (%s)\n%s",
			 i.link->id, ptr2str(&s), wvcrash_read_will());
        #endif

	if (s.isok())
	{
	    if (s.post_select(si))
	    {
		sure_thing.unlink(&s); // don't add it twice!
		sure_thing.append(&s, false, i.link->id);
	    }
	    else
	    {
		WvIStreamListBase::Iter j(sure_thing);
		WvLink* link = j.find(&s);

                #if I_ENJOY_FORMATTING_STRINGS
		wvassert(!link, "stream \"%s\" (%s) was ready in "
			 "pre_select, but not in post_select",
			 link->id, ptr2str(link->data));
                #endif
	    }
	}
	else
	    already_sure = true;
    }
    
    si.wants = oldwant;
    return already_sure || !sure_thing.isempty();
}


// distribute the callback() request to all children that select 'true'
void WvIStreamList::execute()
{
    static int level = 0;
    const char *id;
    level++;
    
    WvStream::execute();
    
    TRACE("\n%*sList@%p: (%d sure) ", level, "", this, sure_thing.count());
    
    WvIStreamListBase::Iter i(sure_thing);
    for (i.rewind(); i.next(); )
    {
#if STREAMTRACE
	WvIStreamListBase::Iter x(*this);
	if (!x.find(&i()))
	    TRACE("Yikes! %p in sure_thing, but not in main list!\n",
		  i.cur());
#endif
	IWvStream &s(*i);
	
	id = i.link->id;

	TRACE("[%p:%s]", &s, id);
	
	i.xunlink();
	
	if (s.isok())
        {
#if DEBUG
            if (!RUNNING_ON_VALGRIND)
            {
                WvString strace_node("%s: %s", s.wstype(), s.wsname());
                ::write(-1, strace_node, strace_node.len()); 
            }
#endif
            #if I_ENJOY_FORMATTING_STRINGS
	    WvCrashWill my_will("executing stream: %s\n%s",
				id ? id : "unknown stream",
				wvcrash_read_will());
            #endif
	    s.callback();
        }
	
	// list might have changed!
	i.rewind();
    }
    
    sure_thing.zap();

    level--;
    TRACE("[DONE %p]\n", this);
}

#ifndef _WIN32
void WvIStreamList::onfork(pid_t p)
{
    if (p == 0)
    {
        // this is a child process: don't inherit the global streamlist
        globallist.zap(false);
    }
}
#endif


void WvIStreamList::add_debugger_commands()
{
    WvStreamsDebugger::add_command("globallist", 0, debugger_globallist_run_cb, 0);
}


WvString WvIStreamList::debugger_globallist_run_cb(WvStringParm cmd,
    WvStringList &args,
    WvStreamsDebugger::ResultCallback result_cb, void *)
{
    debugger_streams_display_header(cmd, result_cb);
    WvIStreamList::Iter i(globallist);
    for (i.rewind(); i.next(); )
        debugger_streams_maybe_display_one_stream(static_cast<WvStream *>(i.ptr()),
                cmd, args, result_cb);
    
    return WvString::null;
}

