# Regression test

For issue `[no issue]`.

The following error messages were encountered:

```
[ storage  ][ error]: error finding catalog entry for bundle identified by acs custody signal
[ storage  ][ warning]: can't find custody timer associated with bundle identified by acs custody signal 
[ storage  ][ error]: could not insert custody id into finalDestNodeIdToOpenCustIdsMap
```

They do not necessarily happen in this order, these are just the three that
were observed.

The first happens when storage is processing a received ACS signal.
ACS signals have custody IDs in them, which are used to track the bundle(s)
with custody. Since we see this error message, it means we don't have a
bundle in our catalog with that custody ID.

The second is similar; it happens when trying to delete a bundle due to a
received ACS signal. Since we see this message, there is not a custody timer
running for that bundle.

The third one is a bit different. The `finalDestNodeIdToOpenCustIdsMap` is used
to track bundles that we've sent to egress for which we are still waiting for an
ack.
Since we cannot insert the ID into this map, it must already be there.

# Running

Requires the `scapy` python package.
How to install this depends on your system.
See scapy docs.

To run, set the variable `HDTN_SOURCE_ROOT`.

Then, 

```sh
./test.py
```

Uses the unit testing framework in python.

# Test Brainstorming

To get these error messages to appear, we need to have two things happen:
1. The custody timer expires while a bundle sits in the egress queue,
   so that it gets set to egress again, giving us that map log print
2. As a result of multiple of the same bundle being sent to egress, we
   want to get back two aggregate custody signals for the same bundle.
   That will give us the first two error messages.

For this then, we need an HDTN and bpsink talking to each other.
The first will have the egress queueing issue, the second will send back
aggregate custody signals.

I think we'll use UDP for this. It's the simplest and we can easily set
the rate limit.

+ Starts HDTN instance
+ TODO detailed description
