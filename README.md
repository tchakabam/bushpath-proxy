# A proxy for the bush! 

## C library and command line tool that emulates network bandwidth and latency for HTTP clients.

# Setup

You need:

+ GLib 2.0
+ GIO library (comes with GLib usually)

## On OSX:

You can just `brew install glib` (or install the GStreamer SDK framework).

## On Linux:

Use your favorite package manager to get GLib development files and binaries (headers + static lib archive).

## Other platforms:

Have a look at https://wiki.gnome.org/Projects/GLib/SupportedPlatforms 
and http://www.gtk.org/download/index.php.

# Build

NOTE: _If you happen to be on OSX and have the **GStreamer framework** already installed, 
      then just run all these make commands with `OSX_GSTREAMER=yes` prepended.
      Thus you will use the headers / binaries included in the framework already 
      and don't need a further install of GLib somewhere else._ Example: 
      ```
      OSX_GSTREAMER=yes make build
      ```

### Clean & Build

```
make
```

### Build

```
make build
```

### Build & Run

```
make start
```

### Clean

```
make clean
```

### Run tool

```
./proxy
```

