Monitor State
=============

This is a simple utility to monitor the system state of an Excito Bubba2 or B3.

Using DBus, the utility connects to Systemd and regularly gather the system state. Depending on the result, the color of the front led is changed.

Led is purple during boot and shutdown (provided these steps are long enough, state is polled every half second by default). On error, the led is red, and on healthy system it is blue.

To compile, use the usual autotools

    bash autogen.sh
    ./configure
    make
    make install
