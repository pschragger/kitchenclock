#!/bin/sh

gcc digitalclock.c -o a.out
cp a.out /usr/sbin/digitalclock
