.PHONY: all stl

COMPONENTS := pico-cec-base.stl pico-cec-middle.stl pico-cec-top.stl pico-cec-pusher.stl

%.stl: %.scad pico-cec-lib.scad
	openscad -o $@ $<

pico-cec.tar.gz: ${COMPONENTS}
	@tar czvf $@ $^

all: stl pico-cec.tar.gz

stl: ${COMPONENTS} pico-cec.stl
