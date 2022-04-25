#!/bin/bash

set -e

meson subprojects download
rm subprojects/*.wrap
mv subprojects /
chmod -R o+r /subprojects