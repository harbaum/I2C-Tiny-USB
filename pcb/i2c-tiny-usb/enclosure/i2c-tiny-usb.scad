$fn=50;

box_width_outer = 35;
box_length_outer = 50;
box_height_outer = 17;

box_thickness = 2.05;
 PCB_THICKNESS=1.6;

box_width_inner = box_width_outer - box_thickness * 2;
box_length_inner =  box_length_outer - box_thickness * 2;
box_height_inner =  box_height_outer - box_thickness * 1;

echo("W_inner",box_width_inner);
echo("L_inner",box_length_inner);
echo("H_inner",box_height_inner);

translate([0,-26,0]) enclosure();
translate([0,26,0]) lid();

module enclosure()
{
    baseplate_z = box_thickness; //this is the height of the internal bottom wall
     pcb_standoff_height=3;
    difference()
    {
         // outer box
        {
            linear_extrude(box_height_outer)
            {
                minkowski() 
                {
                    square([box_width_outer - box_thickness*2,box_length_outer - box_thickness*2],center=true);
                    circle(box_thickness);
                }
            }
        }

        // hollow inside, this negative cube is a bit higher then the planned internal size, so it 
        // cuts out the top wall as well
        translate([0,0,box_thickness])
        {
            linear_extrude(box_height_inner + box_thickness)
            {
                minkowski()
                {
                    square([box_width_inner - box_thickness,
                            box_length_inner - box_thickness],
                            center=true);
                    circle(box_thickness/2);
                }
            }
        }
        

       
        // cutouts
       
        // USB
        translate([0,box_length_inner/2 + box_thickness/2 , baseplate_z + pcb_standoff_height +PCB_THICKNESS + 2]) 
                        cube([8,box_thickness+1,4],center=true);
        // IDC 
        idc_width=18;
        idc_height=11; // heigher than the actual connector: we want to break through to the top edge
        translate([0, -box_length_inner/2 - box_thickness/2 ,  baseplate_z + pcb_standoff_height + PCB_THICKNESS + idc_height/2 ])  
                        cube([idc_width,box_thickness+0.1,idc_height],center=true);
    }
    //pcb standoffs
    translate([box_width_inner/2 - 3.75, box_length_inner/2 - 13, baseplate_z ]) pcb_standoff(pcb_standoff_height);
    translate([-(box_width_inner/2 - 3.75), -( box_length_inner/2 - 13), baseplate_z]) pcb_standoff(pcb_standoff_height);

    //posts for lid screws
    translate([(box_width_inner/2 - 3.75), -( box_length_inner/2 - 3.75), baseplate_z]) rotate(a=180,v=[0,0,1]) fix_standoff_box();
    translate([-(box_width_inner/2 - 3.75), ( box_length_inner/2 - 3.75), baseplate_z]) fix_standoff_box();
}



module lid()
{
    lid_thickness=1.5;
    recess_thickness=0.5;
    difference()
    {
        //2 plates
        union()
        {
            translate([0,0,0])
            {
                linear_extrude(lid_thickness)
                {
                    minkowski() 
                    {
                        square([box_width_outer - box_thickness*2 ,box_length_outer - box_thickness*2],center=true);
                        circle(box_thickness);
                    }
                }
            }

            translate([0,0,lid_thickness])
            {
                linear_extrude(recess_thickness)
                {
                    minkowski()
                    {
                        square([box_width_inner - box_thickness - 0.3,
                                box_length_inner - box_thickness - 0.3],
                                center=true);
                        circle(box_thickness/2);
                    }
                }
            }
        } //plates

        // screw holes
        translate([(box_width_inner/2 - 3.75), ( box_length_inner/2 - 3.75), -0.1]) countersink_hole();
        translate([-(box_width_inner/2 - 3.75), -( box_length_inner/2 - 3.75), -0.1]) countersink_hole();
        // LED hole
        translate([(box_width_inner/2 - 3.5),-( box_length_inner/2 - 4), -0.1]) cylinder(r=1.5,h=3.1);
    }
}

module countersink_hole()
{
    union()
    {
        cylinder(r1=2.5,r2=1,h=2);
        cylinder(r=1.2,h=4);
    }
}

module pcb_standoff(height)
{
    drill_diameter = 1.5;
    standoff_diameter=5.6;

    difference()
    {
        cylinder(d=standoff_diameter, h=height);
        translate([0,0,0.1])cylinder(d=drill_diameter, h=height+0.1);
    }
}



module fix_standoff_box()
{
    drill_diameter=2.2;
    diameter=5.4;
    height = box_height_inner-1;
    support_side_len=3.75;

    difference()
    {
        union ()
        {
            cylinder(d=diameter, h=height);
            translate([-support_side_len/2,+support_side_len/2, height/2])  cube([support_side_len, support_side_len, height], center=true);
        }
        translate([0,0,1])cylinder(d=drill_diameter, h=height);
    }
}
