/* -*- Mode: C++ -*-
 * Worldvisions Weaver Software:
 *   Copyright (C) 2004-2006 Net Integration Technologies, Inc.
 *
 * Test program which sends a simple signal with one argument (int32: 132). 
 * Best used in conjunction with the dbus-sig-listen program.
 * 
 */ 
#include "wvargs.h"
#include "wvdbusconn.h"
#include "wvistreamlist.h"


int main(int argc, char *argv[])
{
    WvArgs args;
    WvStringList remaining_args;
    args.add_optional_arg("MONIKER");
    args.process(argc, argv, &remaining_args);
    WvString moniker = remaining_args.popstr();

    WvDBusConn *conn = new WvDBusConn(!!moniker ? moniker : "bus:session");
    conn->request_name("ca.nit.MySender");

    // Create a signal, bound for "ca.nit.MyApplication"'s "/ca/nit/foo" 
    // object, with the "ca.nit.foo" interface's "bar" method.
    WvDBusSignal sig("/ca/nit/foo", "ca.nit.foo", "bar");
    sig.append((int32_t)132);
    conn->send(sig);

    WvIStreamList::globallist.append(conn, false, "wvdbus conn");
    
    while (WvIStreamList::globallist.isok())
        WvIStreamList::globallist.runonce();
    
    WVRELEASE(conn);
    return 0;
}
