#ifndef _HDTN_EGRESS_H
#define _HDTN_EGRESS_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "message.hpp"
#include "zmq.hpp"

#define HEGR_NAME_SZ (32)
#define HEGR_ENTRY_COUNT (1 << 20)
#define HEGR_ENTRY_SZ (256)

#define HEGR_FLAG_ACTIVE (0x0001)
#define HEGR_FLAG_UP (0x0002)
#define HEGR_HARD_IFG (0x0004)

#define HEGR_FLAG_UDP (0x0010)
#define HEGR_FLAG_STCPv1 (0x0020)
#define HEGR_FLAG_LTP (0x0040)

namespace hdtn {

class hegr_entry {
   public:
    hegr_entry();

    /**
    Initializes the egress port
    */
    virtual void init(sockaddr_in *inaddr, uint64_t flags);

    /**
    Sets the active label for this instance 
    */
    virtual void label(uint64_t label);

    /**
    Sets the active name for this instance. char* n can be of length HEGR_NAME_SZ - 1.
    */
    virtual void name(char *n);

    /**
    Sets a target data rate for an egress port - most often used in conjunction with HARD_IFG

    This is really only useful when one wants the egress to perform its own rate control - elements
    internal to the cluster perform their own rate limiting through an object specific to that.
    */
    virtual void rate(uint64_t rate);

    /**
    Pure virtual method - handles forwarding a batch of messages to a specific receiver

    @param msg list of buffers to send
    @param sz list of the size of each buffer
    @param count number of buffers to send
    @return number of messages forwarded
    */
    virtual int forward(char **msg, int *sz, int count);

    /**
    Runs housekeeping tasks for a specified egress port
    */
    virtual void update(uint64_t delta);

    /**
    Administratively enables this link

    @return zero on success, or nonzero on failure
    */
    virtual int enable();

    /**
    Administratively disables this link

    @return zero on success, or nonzero on failure
    */
    virtual int disable();

    /**
    Checks to see if the port is currently available for use

    @return true if the port is available (ACTIVE & UP), and false otherwise 
    */
    virtual bool available();

    void shutdown();

   protected:
    a
        uint64_t _label;
    uint64_t _flags;
    sockaddr_in _ipv4;
};

class hegr_stcp_entry : public hegr_entry {
   public:
    hegr_stcp_entry();

    /**
    Initializes a new TCP forwarding entry
    */
    void init(sockaddr_in *inaddr, uint64_t flags);

    /**
    Specifies an upper data rate for this link.

    At the moment, HARD_IFG is not supported for TCP applications.

    @param rate Rate at which traffic may flow through this link
    */
    void rate(uint64_t rate);

    /**
    Forwards a collection of UDP packets through this path and to a receiver

    @param msg List of messages to forward
    @param sz Size of each individual message
    #param count Total number of messages
    @return number of bytes forwarded on success, or an error code on failure
    */
    int forward(char **msg, int *sz, int count);

    /**
    Handles housekeeping associated with this link

    @param delta time elapsed since last update()
    */
    void update(uint64_t delta);

    /**
    Calls connect() in a non-blocking fashion.

    Note that success here *does not* indicate that a connection was 
    successfully completed - only that the egress agent began the
    connection process.

    @return zero on success, and nonzero on failure
    */
    int enable();

    /**
    Closes any active connection.  Traffic will not flow through this
    egress port until a new connection has been established (e.g. through
    "enable")

    @return zero on success, and nonzero on failure
    */
    int disable();

    void shutdown();

   private:
    int _fd;
};

class hegr_udp_entry : public hegr_entry {
   public:
    hegr_udp_entry();

    /**
    Initializes a new UDP forwarding entry
    */
    void init(sockaddr_in *inaddr, uint64_t flags);

    /**
    Specifies an upper data rate for this link.  
    
    If HARD_IFG is set, setting a target data rate will disable the use of sendmmsg() and 
    related methods.  This will often yield reduced performance.

    @param rate Rate at which traffic may flow through this link
    */
    void rate(uint64_t rate);

    /**
    Forwards a collection of UDP packets through this path and to a receiver

    @param msg List of messages to forward
    @param sz Size of each individual message
    #param count Total number of messages
    @return number of bytes forwarded on success, or an error code on failure
    */
    int forward(char **msg, int *sz, int count);

    /**
    Essentially a no-op for this entry type
    */
    void update(uint64_t delta);

    /**
    Essentially a no-op for this entry type

    @return zero on success, and nonzero on failure
    */
    int enable();

    /**
    Essentially a no-op for this entry type

    @return zero on success, and nonzero on failure
    */
    int disable();

    void shutdown();

   private:
    int _fd;
};

class hegr_manager {
   public:
    ~hegr_manager();

    /**
     
    */
    void init();

    /**
     
    */
    int forward(int fec, char *msg, int sz);

    /**
     
    */
    int add(int fec, uint64_t flags, const char *dst, int port);

    /**
     
    */
    int remove(int fec);

    /**
     
    */
    void up(int fec);

    /**
     
    */
    void down(int fec);

    bool test_storage = false;

   private:
    hegr_entry *_entry(int offset);
    void *_entries;
};

}  // namespace hdtn

#endif
