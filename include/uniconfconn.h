#ifndef __UNICONFCONN_H
#define __UNICONFCONN_H

#include "wvstreamclone.h"
#include "wvbuffer.h"

class UniConfConn : public WvStreamClone
{
public:
    UniConfConn(WvStream *_s);
    virtual ~UniConfConn();

    WvString gettclline();
    virtual void fillbuffer();
    virtual void execute();
    virtual bool isok() const;

    // CONSTANTS FOR CONTROL CONNECTIONS TO THE DAEMON
    static const WvString UNICONF_GET;
    static const WvString UNICONF_SET;
    static const WvString UNICONF_SUBTREE;
    static const WvString UNICONF_RECURSIVESUBTREE;
    
    static const WvString UNICONF_RETURN;
    static const WvString UNICONF_FORGET;
    static const WvString UNICONF_SUBTREE_RETURN;
    static const WvString UNICONF_OK;
    static const WvString UNICONF_FAIL;

    static const WvString UNICONF_QUIT;
protected:

    WvDynamicBuffer incomingbuff;
private:
};

#endif // __UNICONFCONN_H
