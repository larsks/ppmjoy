# ppmjoy

> [!NOTE]
> 
> This is a fork of [ppm][ppm_repo], by Jason McCarty. Jason's [original
> license] is not open source; I have [contacted Jason] about re-licensing the
> original project with an open source license. Depending on Jason's response
> this fork may disappear.

[ppm_repo]: https://github.com/jbmccarty/ppm
[original license]: https://github.com/jbmccarty/ppm/blob/d9bd68fcb1fe0ef62fcacf88b5f7d64fd22e9181/LICENSE
[contacted jason]: https://github.com/jbmccarty/ppm/issues/1

This software creates a joystick device on Linux, whose positions are
derived from a [PPM] (pulse-position modulation) signal with up to 8 channels on
the default ALSA device on Linux.

The resulting device will be available as a joystick device
(`/dev/input/js<N>`) and as an input device (`/dev/input/event<N>`). For
testing the behavior, use the `evtest` program, available from
https://cgit.freedesktop.org/evtest (and probably already packaged in your
distribution).

[PPM]: https://en.wikipedia.org/wiki/Pulse-position_modulation

## Usage

```
ppmjoy: usage: ppmjoy [--device|-d alsa_device] [--config|-f ppmjoy_config]
```

### Options

- `--device/-d` -- specify the ALSA device from which to read the PPM input.
E.g. `-d hw:2`.

- `--config/-f` -- specify the file from which to read the `ppmjoy`
configuration. This will default to the value of the `PPMJOY_CONFIG`
environment variable, or if that is not set to `~/.config/ppmjoy.toml`.

- `--monitor|-m` -- live output of channel values.

- `--verbose|-v` -- enable some verbose logging. May be specified multiple times for increased verbosity.

## Configuration

The `ppmjoy` configuration file describes the relationship between PPM channels
and input events. The configuration file is a TOML file with channel
configurations specified as an array of tables. For example, the following
example matches the [default configuration](#default-configuration), which maps
PPM channels 1-4 onto input axes:

```toml
[[channels]]
type = "axis"
code = "ABS_RX"

[[channels]]
type = "axis"
code = "ABS_RY"

[[channels]]
type = "axis"
code = "ABS_Y"

[[channels]]
type = "axis"
code = "ABS_X"
```

A channel configuration can be one of three different types, described in the follow sections.

### Axis

An `axis` controls maps the channel value directly to the named axis.

### Button

A `button` control maps the channel value to button press/release events. This is good for momentary switches on your transmitter. For example, I use the following configuration to map a momentary switch to event `BTN_3`, which I use to toggle the viewpoint in Liftoff:

```toml
# viewpoint
[[channels]]
type = "button"
code = "BTN_3"
threshold = 250
```

When the channel values becomes greater than 250, this generates a button press event. When it falls below 250, this triggers a button release.


### Multi

A `multi` control maps the channel value to multiple buttons. As the value crosses defined thresholds, `ppmjoy` will generate button press and button release events for the corresponding button. For example, there is a three-position switch on my transmitter that I would like to use to control the flight mode in Liftoff. I use the following configuration:

```toml
# flight mode
[[channels]]
type = "multi"
num_positions = 3
thresholds = [250, 350]
codes = ["BTN_5", "BTN_6", "BTN_7"]
hysteresis = 10
```

The values produced by the channel mapped to this button are approximately 188 in position, 288 in position 2, and 388 in position 3. I have set the thresholds slightly below those values in case there is any jitter on the channel. With the above configuration, moving the switch into position will generate button press and button release events for `BTN_5`. When moving the switch into the center position, it will generate press and release events for `BTN_6`, etc. This allows me to set up Liftoff with the following control mapping:

| Action              | Button   |
| ------------------- | -------- |
| Toggle LEVEL mode   | Button5  |
| Toggle ACRO mode    | Button6  |
| Toggle Horizon mode | Button7  |

A `multi` control can have up to 4 positions. In this example I'm configuring a multi-position switch, but you could just as easily use this technique to map a dial to up to 4 different buttons.

### Examples

The file [example_config.toml](example_config.toml) has a complete example configuration.

### Default configuration

In the absence of a configuration file, `ppmjoy` will use the following default configuration:

| Channel | Mapping |
| ------- | ------- |
| 0       | ABS_X   |
| 1       | ABS_Y   |
| 2       | ABS_RX  |
| 3       | ABS_RY  |

## Permissions

The program needs permission to read from the ALSA device, and to create [uinput] devices. There are several ways of ensuring that `ppmjoy` has appropriate permissions:

[uinput]: https://www.kernel.org/doc/html/v4.12/input/uinput.html

1. Many distributions provide an `audio` group with appropriate permissions on
   sound devices. There's a good chance you're already a member of that group,
since access to sound devices is required for things like watching cat videos
on YouTube.

1. `/dev/uinput` often defaults to `root`-only access. You can create [udev]
   rules to change the group and permissions of `/dev/uinput`. You can find
example rules in the file [ppmjoy.rules](ppmjoy.rules) that will make
`/dev/uinput` writeable by group `input` (this group must of course exist on
your system). Install this file in  `/etc/udev/rules.d/99-ppmjoy.rules`, and
then run:

    [udev]: https://www.freedesktop.org/software/systemd/man/latest/udev.html

    ```
    udevadm control --reload
    udevadm trigger
    ```

1. You can run `ppmjoy` as root, but why would you do that when there are better options available?


## License

This license for this project is in flux; see [this issue][contacted jason].

This project makes use of [tomlc99](https://github.com/cktan/tomlc99), which is licensed under the terms of the MIT license:

```
MIT License

Copyright (c) CK Tan
https://github.com/cktan/tomlc99

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
