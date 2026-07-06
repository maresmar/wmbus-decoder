---
name: flipper-selftests
description: Fetch selftest results from flipper
---

Deploy the app to the flipper, wait for 2s after deployment for app to start and fetch selftest results from the device using:

```bash
port=/dev/<port>
stty -F $port 230400 raw -echo -echoe -echok

cat $port & reader=$!

sleep 1
printf 'storage read /ext/apps_data/wmbus_decoder/selftest.txt\r\n' > $port

sleep 1
kill "$reader"
```

Replace `<port>` with port from `ufbt launch` output. Device can't change easily port so if the port isn't available the device was disconnected.

Expected result ends with:

```text
selftests done total=58 passed=58 failed=0
```


## Sandbox / Device Access

Flipper serial devices such as `/dev/ttyACM0` and `/dev/ttyACM1` may not be visible inside the default sandbox.
