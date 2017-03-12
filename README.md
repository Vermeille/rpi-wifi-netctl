# Wificonf

A simple, lightweight, embeddable webserver for Archlinux ARM that allows to
configure wifi networks for netctl.

# Dependencies

* libmicrohttpd-dev to build, libmicrohttpd on the host.
* Any C++1z capable compiler

# How to use

* Predefine in your /etc/netctl a configuration network.
* Add a systemd unit file to autolaunch the wificonf executable.
* Tether that predefined network from your phone and wait for your rpi to
  connect to it.
* Access the IP address of your rpi from your phone, in a browser.
* Add any network you like.
* Enjoy
