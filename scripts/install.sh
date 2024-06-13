#!/bin/sh

sudo cp digitalclock /usr/sbin/digitalclock
sudo cp diggitalclock.service /etc/systemd/system/digitalclock.service
sudo systemctl enable digitalclock 

