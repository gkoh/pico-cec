$fa = 1;
$fs = $preview ? 2 : 0.5;

module hdmi_2d_male() {
    union () {
        hull() {
            square([13.9, 2.8], true);
                translate([0,-(2.8+0.35)/2])
            square([11.45, 0.35], true);
        }
        hull () {
            translate([0,-(2.8+0.35)/2])
                square([11.45, 0.35], true);
            translate([0, -(4.45)/2])
                square([9.5, 1.3], true);
        }
    }
}

module hdmi_2d_female() {
    scale([15.0/13.9, 5.3/4.45])
        hdmi_2d_male();
}

module hdmi_male() {
    color("goldenrod") union() {
        linear_extrude(12.7)
            hdmi_2d_male();
        translate([0, -1, 12])
            cube([14.2, 5.0, 3],true);
    }
}

module hdmi_female() {
    color("red") linear_extrude(11.4)
        hdmi_2d_female();
}

module hdmi_adapter() {
    union() {
        color("green") difference() {
            cube([27.8, 29.6, 1.6], true);
            translate([11, (29.6/2) - 6, -1.5])
                cylinder(3, r=1.5);
            translate([-11, (29.6/2) - 6, -1.5])
                cylinder(3, r=1.5);
            translate([11, -((29.6/2) - 6), -1.5])
                cylinder(3, r=1.5);
            translate([-11, -((29.6/2) - 6), -1.5])
                cylinder(3, r=1.5);

        }
        translate([0, -(29.6/2), (4.45 - 2.8)/2])
            rotate([90, 0, 0])
                hdmi_female();
        translate([0, (29.6/2) + 12.7, (4.45 - 2.8)/2])
            rotate([90, 0, 0])
                hdmi_male();
    }
}

module usb_c_female() {
    color("silver") linear_extrude(7.2) {
        hull() {
            translate([4.55-1.6, 0, 0])
                circle(r=1.6);
            translate([-4.55+1.6, 0, 0])
                circle(r=1.6);
        }
    }
}

module xiao_rp2040() {
    union() {
        color("blue") cube([17.5, 21.0, 1.2]);
        translate([17.5/2, 5, 1.6+1.0])
            rotate([90,0,0])
                usb_c_female();
    }
}

module nut_m3() {
    cylinder(2.5, r=3.3, $fn=6);
}

module head_m3() {
    cylinder(2.5, r=3);
}

module base() {
    difference() {
        union() {
            difference() {
                cube([32, 40.5+1.5, 9], true);
                translate([0, 0, 1.5])
                    cube([32-3, 40.5+1.5-3, 9-1.5], true);
                translate([0, 4.1, 1.5])
                    hdmi_adapter();
                translate([0, 20, 5])
                    cube([14.1, 3, 3], true);
                translate([0, -20, 5])
                    cube([15.0, 3, 3], true);
            }
            // posts
            translate([11, 13.0, -4.5])
                cylinder(5, r=5);
            translate([11, -4.0, -4.5])
                cylinder(5, r=5);
            translate([-11, 13.0, -4.5])
                cylinder(5, r=5);
            translate([-11, -4.0, -4.5])
                cylinder(5, r=5);
        }
        // bolt
        translate([11, 13.0, -4.55])
            cylinder(5.1, r=1.6);
        translate([11, -4.0, -4.55])
            cylinder(5.1, r=1.6);
        translate([-11, 13.0, -4.55])
            cylinder(5.1, r=1.6);
        translate([-11, -4.0, -4.55])
            cylinder(5.1, r=1.6);
        // nuts
        translate([11, 13.0, -4.55])
            nut_m3();
        translate([11, -4.0, -4.55])
            nut_m3();
        translate([-11, 13.0, -4.55])
            nut_m3();
        translate([-11, -4.0, -4.55])
            nut_m3();
    }
}

module middle() {
    union() {
        difference() {
            cube([32, 40.5+1.5, 4], true);
            // bolt
            translate([11, 13.0, -2.55])
                cylinder(5.1, r=1.6);
            translate([11, -4.0, -2.55])
                cylinder(5.1, r=1.6);
            translate([-11, 13.0, -2.55])
                cylinder(5.1, r=1.6);
            translate([-11, -4.0, -2.55])
                cylinder(5.1, r=1.6);
            // head
            translate([-11, 13.0, -0.45])
                head_m3();
            translate([11, 13.0, -0.45])
                head_m3();
            // xiao rp2040 pins
            translate([7.5, -9.1, 0])
                cube([2.5, 21, 4.05], true);
            translate([-7.5, -9.1, 0])
                cube([2.5, 21, 4.05], true);
            // hdmi adapter pins
            translate([0, 4, 0])
                cube([27.8, 8, 4.05], true);
        }
        // usb-c port bottom half
        translate([0, -20.25, (4+2.9)/2])
            difference() {
                cube([20.5, 1.5, 2.9], true);
                translate([0, 5, 2.9/2 ])
                    rotate([90, 0, 0])
                        usb_c_female();
            }
         // side supports
         translate([-9.6, -14.5, (4+2.9)/2])
            cube([1.3, 12, 2.9], true);
         translate([9.6, -14.5, (4+2.9)/2])
            cube([1.3, 12, 2.9], true);
        }
}

module top() {
    difference() {
        cube([32,30, 9], true);
        translate([0, -2.5, -3])
            cube([32-3, 30-8.5, 9-3], true);
        translate([0, -13.25, -1.5 ])
            rotate([90, 0, 0])
                usb_c_female();
        translate([0, -14.05, -3])
            cube([20.5, 2, 3.05], true);
        // bolt
        translate([-11, 2, -0.5])
            cylinder(5.1, r=1.6);
        translate([11, 2, -0.5])
            cylinder(5.1, r=1.6);
        // head
        translate([-11, 2, 2.5])
            head_m3();
        translate([11, 2, 2.5])
            head_m3();
        // led/button slot
        translate([0, 6.7, 2.2])
            cube([15, 3, 5], true);
        // power led
        translate([-6, -10, -0.3])
            cylinder(5, r=1.5);
    }
}

//base();
//middle();
//top();
