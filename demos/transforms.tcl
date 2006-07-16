package require tkpathset t .c_transformstoplevel $tset w $t.cpack [canvas $w -bg white -width 480 -height 300]set mskewx1 [::tkpath::transform skewx 0.3]set mskewx2 [::tkpath::transform skewx 0.5]set mrot    [::tkpath::transform rotate [expr 3.1415/4] 100 100]set g1 [::tkpath::lineargradient create -stops {{0 lightblue} {1 blue}}]$w create path "M 10 10 h 200 v 50 h -200 z" -fillgradient $g1 -matrix $mskewx1set g2 [::tkpath::lineargradient create -stops {{0 #f60} {1 #ff6}}]$w create path "M 10 70 h 200 v 50 h -200 z" -fillgradient $g2 -matrix $mrotset g4 [::tkpath::lineargradient create -stops {{0 white} {0.5 black} {1 white}}]$w create path "M 10 220 h 200 v 50 h -200 z" -fillgradient $g4 -matrix $mskewx2if {![tkpath::pixelalign]} {    $w move all 0.5 0.5}