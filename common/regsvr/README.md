## HDTN Registration Server

This is a Python application that acts as a central registry for running HDTN applications (services).  

0mq is supported at present, though HTTP would be a straightforward addition (and thus may be supported in the future).

NOTE: At present, registrations do not persist across subsequent executions.

## Messages

Messages are plaintext.  Message format is:

HDTN/1.0 COMMAND ARGUMENTS

where COMMAND is one of:

_REGISTER_ - registers a new instance of a service

_DEREGISTER_ - removes an instance of a service

_QUERY_ - searches for all known services

Responses to commands are of the form:

HDTN/1.0 CODE DESCRIPTION | SUPPLEMENTARY_DATA

Where:

_CODE_ is a numerical value describing the result of the operation,

_DESCRIPTION_ is a textual description of the result of the operation, and

_SUPPLEMENTARY\_DATA_ contains any additional data relevant to a user's request

## Workflow

### Registration

Upon startup, an HDTN service is expected to _REGISTER_.  A registration will include a 0mq Identity property string set to the following:

service_type:service_port:comm_type

Where:

_service\_type_ - String describing the category of the service.  For example, _ingress_, _egress_, _telemetry_, or _storage_.
_service\_port_ - Numerical value indicating the port upon which the service is listening.  Expected range is 10430 + X, where X is as low as possible based on service allocation.
_comm\_type_ - Type of communication expected - should map to a 0mq socket type (e.g. REQ, PUB, PUSH).  Pair-wise type should be used to connect: for example, if _comm\_type_ is "PUSH", a "PULL" socket should be used to connect to the service.

This Identity string, in combination with the Peer-Address string, will be used to construct an entry for an active HDTN service.

### Query

Any HDTN service can execute a QUERY to search for other running services.  For example, a storage service would execute a QUERY to look for active egress service instances.  It could use the retrieved registration data to connect to such a service and begin receiving data.

The QUERY command supports two modes of operation: one with no arguments, and another with a single argument.  If no arguments are included, QUERY returns a complete (JSON-encoded) list of results as _SUPPLEMENTARY\_DATA_.

### Deregistration

When a service is shutting down, it is expected to _DEREGISTER_.  Parameters are identical to the Registration operation described above.

## C++ binding

The "lib" directory includes a C++ library that will connect to the registration server and execute the commands specified above.  To build
this library, use the standard "mkdir build && pushd build && cmake .. && make".  

The build will produce a "libhdtn_reg.a" library, which should be linked into the target HDTN service.  Additionally, the "hdtn_reg.h" include file should be made available in the HDTN service's include search path for the compilation stage of the project.  There is a small test application that is built as part of the compilation process as well.

An example of using this library can be found below:

```c++
#include "reg.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    hdtn::hdtn_regsvr regsvr;
    regsvr.init("tcp://127.0.0.1:10140", "test", 10141, "PUSH");
    regsvr.reg();
    hdtn::hdtn_entries res = regsvr.query();
    for(auto entry : res) {
       std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
    }
    regsvr.dereg();
    return 0;
}
```
