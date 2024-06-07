.PHONY: all stl

%.stl: %.scad
	openscad -o $@ $<

all: stl

stl: pico-cec-base.stl pico-cec-middle.stl pico-cec-top.stl
