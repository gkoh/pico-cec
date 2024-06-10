.PHONY: all stl

%.stl: %.scad pico-cec-lib.scad
	openscad -o $@ $<

all: stl

stl: pico-cec-base.stl pico-cec-middle.stl pico-cec-top.stl
