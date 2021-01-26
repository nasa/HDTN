#ifndef _HDTN_EGRESS_H
#define _HDTN_EGRESS_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include "message.hpp"
#include "paths.hpp"
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

class HegrEntry {
 public:
  // JCF, seems to be missing virtual destructor, added below
  virtual ~HegrEntry();

  HegrEntry();

  /**
  Initializes the egress port
  */
  virtual void Init(sockaddr_in *inaddr, uint64_t flags);

  /**
  Sets the active label for this instance
  */
  virtual void Label(uint64_t label);

  /**
  Sets the active name for this instance. char* n can be of length HEGR_NAME_SZ
  - 1.
  */
  virtual void Name(char *n);

  /**
  Sets a target data rate for an egress port - most often used in conjunction
  with HARD_IFG

  This is really only useful when one wants the egress to perform its own rate
  control - elements internal to the cluster perform their own rate limiting
  through an object specific to that.
  */
  virtual void Rate(uint64_t rate);

  /**
  Pure virtual method - handles forwarding a batch of messages to a specific
  receiver

  @param msg list of buffers to send
  @param sz list of the size of each buffer
  @param count number of buffers to send
  @return number of messages forwarded
  */
  virtual int Forward(char **msg, int *sz, int count);

  /**
  Runs housekeeping tasks for a specified egress port
  */
  virtual void Update(uint64_t delta);

  /**
  Administratively enables this link

  @return zero on success, or nonzero on failure
  */
  virtual int Enable();

  /**
  Administratively disables this link

  @return zero on success, or nonzero on failure
  */
  virtual int Disable();

  /**
  Checks to see if the port is currently available for use

  @return true if the port is available (ACTIVE & UP), and false otherwise
  */
  virtual bool Available();

  void Shutdown();

 protected:
  uint64_t label_;
  uint64_t flags_;
  sockaddr_in ipv4_;
};

class HegrStcpEntry : public HegrEntry {
 public:
  HegrStcpEntry();

  /**
  Initializes a new TCP forwarding entry
  */
  void Init(sockaddr_in *inaddr, uint64_t flags);

  /**
  Specifies an upper data rate for this link.

  At the moment, HARD_IFG is not supported for TCP applications.

  @param rate Rate at which traffic may flow through this link
  */
  void Rate(uint64_t rate);

  /**
  Forwards a collection of UDP packets through this path and to a receiver

  @param msg List of messages to forward
  @param sz Size of each individual message
  @param count Total number of messages
  @return number of bytes forwarded on success, or an error code on failure
  */
  int Forward(char **msg, int *sz, int count);

  /**
  Handles housekeeping associated with this link

  @param delta time elapsed since last update()
  */
  void Update(uint64_t delta);

  /**
  Calls connect() in a non-blocking fashion.

  Note that success here *does not* indicate that a connection was
  successfully completed - only that the egress agent began the
  connection process.

  @return zero on success, and nonzero on failure
  */
  int Enable();

  /**
  Closes any active connection.  Traffic will not flow through this
  egress port until a new connection has been established (e.g. through
  "enable")

  @return zero on success, and nonzero on failure
  */
  int Disable();

  void Shutdown();

 private:
  int fd_;
};

class HegrUdpEntry : public HegrEntry {
 public:
  HegrUdpEntry();

  /**
  Initializes a new UDP forwarding entry
  */
  void Init(sockaddr_in *inaddr, uint64_t flags);

  /**
  Specifies an upper data rate for this link.

  If HARD_IFG is set, setting a target data rate will disable the use of
  sendmmsg() and related methods.  This will often yield reduced performance.

  @param rate Rate at which traffic may flow through this link
  */
  void Rate(uint64_t rate);

  /**
  Forwards a collection of UDP packets through this path and to a receiver

  @param msg List of messages to forward
  @param sz Size of each individual message
  @param count Total number of messages
  @return number of bytes forwarded on success, or an error code on failure
  */
  int Forward(char **msg, int *sz, int count);

  /**
  Essentially a no-op for this entry type
  */
  void Update(uint64_t delta);

  /**
  Essentially a no-op for this entry type

  @return zero on success, and nonzero on failure
  */
  int Enable();

  /**
  Essentially a no-op for this entry type

  @return zero on success, and nonzero on failure
  */
  int Disable();

  void Shutdown();

 private:
  int fd_;
};

class HegrManager {
 public:
  ~HegrManager();

  /**

  */
  void Init();

  /**

  */
  int Forward(int fec, char *msg, int sz);

  /**

  */
  int Add(int fec, uint64_t flags, const char *dst, int port);

  /**

  */
  int Remove(int fec);

  /**

  */
  void Up(int fec);

  /**

  */
  void Down(int fec);

  bool test_storage_ = false;
  const char *cut_through_address_ = HDTN_CUT_THROUGH_PATH;
  const char *release_address_ = HDTN_RELEASE_PATH;
  zmq::context_t *zmq_cut_through_address_;
  zmq::socket_t *zmq_cut_through_sock_;
  zmq::context_t *zmq_release_ctx_;
  zmq::socket_t *zmq_release_sock_;

 private:
  HegrEntry *entry_(int offset);
  void *entries_;
};

}  // namespace hdtn

#endif
