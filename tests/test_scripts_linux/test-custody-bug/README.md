# Regression test

For issue #206.

Segfault when handling custody.

Requires the `scapy` python package.
How to install this depends on your system.
See scapy docs.

To run, set the variable `HDTN_SOURCE_ROOT`.

Then, 

```sh
./test.py
```

Uses the unit testing framework in python.

## Test description

+ Starts HDTN instance
+ Sends a bundle that requests custody to HDTN
+ Waits for HDTN to send the custody signal
  and for HDTN to forward the bundle
+ Removes all contacts from HDTN (so that it
  cannot re-send the bundle)
  + This prevents the bundle from moving out
    of the awaiting-send state after it
    re-enters it
+ Waits for the custody timer to expire for
  the bundle (at this point the bundle is in
  the awaiting-send state)
+ Sends custody accepted signal to HDTN
  + This deletes the bundle from some of the
    catalog data structures but not all of them
+ Does a storage API request to trigger the crash
