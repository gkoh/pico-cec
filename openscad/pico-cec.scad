include <pico-cec-lib.scad>

// almost sandwiched
//base();
//translate([0, 4, 2.3])
//    hdmi_adapter();
//translate([-17.5/2, -19.5, 10])
//    rotate([0, 0, 0])
//        xiao_rp2040();
//translate([0, 0, 7])
//    middle();
//translate([0, -6, 13.6])
//    top();

// slightly exploded
base();
translate([0, 4, 8])
    hdmi_adapter();
translate([0, 0, 14])
    middle();
translate([-17.5/2, -19.5, 18])
    rotate([0, 0, 0])
        xiao_rp2040();
translate([0, -6, 24])
    top();
