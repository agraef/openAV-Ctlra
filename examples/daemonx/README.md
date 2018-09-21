# daemonx (extended daemon example)

This is an attempt to rework the ctlra daemon example so that it becomes more useful. It also serves as a showcase utilizing most ctlra features, and as a hacking playground to explore the capabilities of the current flock of NI Maschine devices.

In particular, this version of the daemon adds the capability to output text on the screen, improves the local feedback, and also adds optional MIDI and MCP feedback, including support for scribble strips, timecode and meter displays. For good measure, it also throws in some external configuration capabilities and a screenshot utility via custom sysex messages. It also adds a Ctlra prefix to the ALSA client name, so that it can be distinguished more easily from the dummy MIDI device created by ALSA itself.

**NOTE:** This hasn't been tested with anything but the Maschine Mk3 yet; it hopefully works at least to some extent with the other devices supported by ctlra, but probably needs some (or a lot of) work, depending on the particular device. Also, there's no mappa support yet, so to actually make it work as a kind of Mackie emulation with the Maschine Mk3, you currently need to use [midizap](https://github.com/agraef/midizap) with the Maschine.midizaprc configuration.

The feedback options are all disabled by default, so daemonx will look pretty much like the daemon example on your device as long as you don't use these options. The screen output is tailored to the Maschine Mk3 right now. Obviously, you won't be able to see the text output and the MCP feedback on the screen if your device doesn't have one, or the display may look garbled if your device has a different screen size. Other than that, daemonx should work at least as well as the daemon example with most devices, but offers a number of interesting additional configuration options (see below), even if you don't utilize the MCP feedback and text output capabilities.

## Usage

ctlra_daemonx [-h] [-d *name*] [-f[*opts*]] [-i *opts*] [-n *count*] [-q]

## Options

-h
:   Print a short help message and exit.

-d *name*
:   Connect only to devices matching the given name; *name* can also be the vendor:device spec in hex, and may contain shell wildcards.

-f[*opts*]
:   Enable feedback (l, n or m; default: all), the available options are: l = local feedback on device (pushed buttons light up); n = normal feedback via MIDI input (MIDI notes light up buttons); m = MCP (Mackie) feedback via MIDI input (timecode, meterbridge, etc.)

-i *opts*
:   Disable individual feedback items, the available options are: g = grid, b = buttons, c = other controls, x = text, t = timecode, s = scribble/meter strips, r = RSM indicators

-n *count*
:   Set the maximum number of devices to open. This is often used in conjunction with `-d`. E.g., `-d 'Maschine*' -n1` will open the first available Maschine device only.

-q
:   Quit automatically if no devices are present, or if the device count drops to zero during operation (usually because devices are disconnected at run time).
