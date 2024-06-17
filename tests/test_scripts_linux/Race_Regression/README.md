# Reproduce race condition

1. Run ./runscript.sh to run hdtn, bpgen, and bpsink
2. Open web gui, observe data flowing through storage
3. Run ./reloader.py to send a contact plan update
4. If bug present, should break things
