# Button Adapter

`ButtonAdapter` samples the active-low BOOT button on GPIO0 through the board
descriptor and emits semantic events through a non-owning callback. It never
changes relay state, erases configuration, restarts the device, or performs
application logic.

Call `update(nowMs)` every 10 ms. The default gestures are:

- release before 3 seconds: identify/status request;
- release after 3 seconds: commissioning request;
- hold for 10 seconds: factory-reset armed event for visible confirmation;
- release after arming: factory-reset request.

Input is ignored during the first second after initialization. A button held
during that qualification interval is suppressed until released, preventing a
GPIO0 boot/download action from becoming an application gesture.