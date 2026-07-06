---
name: deploy-to-flipper
description: Deploys a Flipper app to the device and fetch logs
---

Build the app into the fap, and then deploy it using the Flipper CLI. 

```bash
ufbt launch
```

Take a note about port used
```
        APPCHK  ~/.ufbt/build/wmbus_decoder.fap
                Target: 7, API: 87.1
2026-07-04 21:07:32,311 [INFO] Using flip_<name> on /dev/<port>
```

## Sandbox / Device Access

Flipper serial devices such as `/dev/ttyACM0` and `/dev/ttyACM1` may not be visible inside the default sandbox.