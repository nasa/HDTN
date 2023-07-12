# Test case for priority enforcement

## Test overview:

1. Start HDTN
2. Use bpsendfile to fill HDTN storage with expedited data (high priority)
3. At the same time:
   * Start bpsendfile sending HDTN normal priority data
   * Start bpreceivefile so that HDTN can begin forwarding data
4. Wait for all data to flow

## Description

The basic idea here is to send HDTN normal priority data while high priority
data is sitting in storage.

Without the enforce priority flag set, HDTN will alternate between sending
from storage and the cut-through queue.

With the enforce priority flag set, HDTN should forward all the expedited
data before the normal data.

The rates set via the `--cla-rate` and in the contact plan are designed
to ensure that HDTN will have both high priority data in storage and
receive normal priority data _at the same time_. This is key for the test
to work.

## Usage

Build HDTN with stats logging enabled. This is necessary to see the
bundle arrival order (and thus test that bundles arrive in the correct
order).

Make sure `HDTN_SOURCE_ROOT` is set in the environment.

Then, run `./test.py`.

The `plot.py` script can be used to visualize the bundle arrivals
and rate.

Use it like this after running a test:

```
./plot.py tmp/stats/bundle_stats/*.csv
```

Try changing the value of `enforceBundlePriority` in the hdtn config at
`configs/hdtn.hson`. The test should fail and the plot should show bundles
arriving out of priority order.
